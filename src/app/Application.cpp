#include "Application.h"
#include "../platform/Logger.h"
#include "../platform/StringUtils.h"
#include "../platform/CrashHandler.h"
#include "../platform/StreamUtils.h"
#include "../source/FileSource.h"
#include "../source/CaptureSource.h"
#include "../source/PlaylistSource.h"
#include "../detect/VideoProcSource.h"
#include "../detect/GrayscaleFilter.h"

#include <windowsx.h>
#include <imm.h>
#include <ShlObj.h>
#include <roapi.h>
#include <vector>
#include <algorithm>
#include <functional>

#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600

// Alt+Esc is registered as a system-wide hotkey. Windows itself does not claim
// this combination, and Alt is required so the hotkey does not swallow a plain
// Esc, which cancels capture picking and stops playback in the focused window.
static constexpr int kCornerQuakeHotkeyId = 1;
static constexpr UINT kCornerQuakeHotkeyVk = VK_ESCAPE;
static constexpr UINT kCornerQuakeModKey = MOD_ALT;

// Repeat handling for the toggle gesture. The hotkey is registered without
// MOD_NOREPEAT: that flag suppresses repeats by watching for a key-up, which a
// keyboard that only reports key-down never sends, so the toggle would fire once
// and then stay locked out. Suppression happens here instead, with two floors:
//   * duplicate: deliveries arriving back-to-back are one gesture.
//   * repeat-grace: while the key still reads as held, a delivery is treated as
//     auto-repeat, but only for this long, so a keyboard that never reports a
//     key-up cannot lock the toggle out forever.
static constexpr int kCornerQuakeDuplicateMs = 150;
static constexpr int kCornerQuakeRepeatGraceMs = 900;

// Where the collapsed player is parked. A window only receives its registered
// hotkey while it is neither hidden nor minimized (measured deliveries: 2/2 when
// visible or parked, 0/2 after SW_HIDE or SW_MINIMIZE), so collapsing must keep
// WS_VISIBLE and the normal display state. WS_EX_TOOLWINDOW plus an off-screen
// position hide it from the user, the taskbar and Alt+Tab instead.
static constexpr int kCornerQuakeParkX = -32000;
static constexpr int kCornerQuakeParkY = -32000;

static bool g_isFullscreen = false;
static RECT g_windowedRect;
static Application* g_app = nullptr;

// --- IUnknown ----------------------------------------------------------------

STDMETHODIMP Application::QueryInterface(REFIID riid, void** ppvObject) {
    if (riid == IID_IUnknown || riid == IID_IDropTarget) {
        *ppvObject = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) Application::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) Application::Release() {
    // Application is a stack object in WinMain(). OLE holds a reference while drag
    // and drop is registered, but this object is never COM-owned and must not
    // delete itself at zero. The count is clamped so a stray Release cannot hand a
    // caller a "destroyed" object that is still in use.
    ULONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) {
        InterlockedIncrement(&m_refCount);
        return 1;
    }
    return count;
}

// --- IDropTarget -------------------------------------------------------------

STDMETHODIMP Application::DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    *pdwEffect = DROPEFFECT_COPY;
    return S_OK;
}

STDMETHODIMP Application::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    *pdwEffect = DROPEFFECT_COPY;
    return S_OK;
}

STDMETHODIMP Application::DragLeave() {
    return S_OK;
}

STDMETHODIMP Application::Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    *pdwEffect = DROPEFFECT_COPY;
    HandleFileDrop(pDataObj);
    HandleTextDrop(pDataObj);
    return S_OK;
}

// --- Drop Handlers -----------------------------------------------------------

void Application::HandleFileDrop(IDataObject* pDataObj) {
    FORMATETC fmt = {};
    fmt.cfFormat = CF_HDROP;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;

    STGMEDIUM medium = {};
    if (FAILED(pDataObj->GetData(&fmt, &medium))) return;

    HDROP hDrop = (HDROP)medium.hGlobal;

    auto* ctrl = m_controller.get();
    if (!ctrl) { ReleaseStgMedium(&medium); return; }

    // Enumerate a directory recursively, appending video files to playlist
    std::function<void(const std::wstring&, std::vector<std::string>&)> enumerateDir;
    enumerateDir = [&enumerateDir](const std::wstring& dir, std::vector<std::string>& playlist) {
        std::wstring searchPath = dir + L"\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;
        do {
            if (wcscmp(fd.cFileName, L".") == 0 ||
                wcscmp(fd.cFileName, L"..") == 0) continue;
            std::wstring fullPath = dir + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                enumerateDir(fullPath, playlist);
            } else {
                std::string utf8Path = w2u(fullPath);
                if (IsVideoFile(utf8Path)) {
                    playlist.push_back(utf8Path);
                }
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    };

    int fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
    std::vector<std::string> playlist;
    bool hasDirectories = false;

    for (int i = 0; i < fileCount; ++i) {
        wchar_t filePath[MAX_PATH];
        DragQueryFile(hDrop, i, filePath, MAX_PATH);

        DWORD attrs = GetFileAttributesW(filePath);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            hasDirectories = true;
            enumerateDir(std::wstring(filePath), playlist);
        } else {
            std::string utf8Path = w2u(filePath);
            if (IsVideoFile(utf8Path)) {
                playlist.push_back(utf8Path);
            }
        }
    }

    if (!playlist.empty()) {
        std::sort(playlist.begin(), playlist.end());
        if (playlist.size() == 1 && !hasDirectories) {
            ctrl->SetSource(WrapSource(std::make_unique<FileSource>(playlist[0].c_str(), m_device)));
        } else {
            ctrl->SetSource(WrapSource(std::make_unique<PlaylistSource>(playlist, m_device)));
        }
    }

    auto* src = ctrl->GetSource();
    if (src) {
        SetWindowTextW(m_window, u2w(src->GetTitle()).c_str());
    }
    ReleaseStgMedium(&medium);
}

void Application::HandleTextDrop(IDataObject* pDataObj) {
    FORMATETC fmt = {};
    fmt.cfFormat = CF_UNICODETEXT;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;

    STGMEDIUM medium = {};
    if (FAILED(pDataObj->GetData(&fmt, &medium))) return;

    const wchar_t* wtext = (const wchar_t*)GlobalLock(medium.hGlobal);
    if (!wtext) { ReleaseStgMedium(&medium); return; }

    std::string text = TrimString(w2u(wtext));
    GlobalUnlock(medium.hGlobal);
    ReleaseStgMedium(&medium);

    if (text.empty()) return;

    auto* ctrl = m_controller.get();
    if (!ctrl) return;

    // A local path and a URL need the same handling (FFmpeg opens either), so the
    // classification only feeds the log: it used to select between two identical
    // branches, which made IsStreamUri() look meaningful when it was not.
    if (IsStreamUri(text)) {
        logger->info("Text drop: opening '{}' as a stream URI", text);
    } else if (!IsVideoFile(text)) {
        logger->warn("Text drop: '{}' has no recognised video extension, trying it anyway", text);
    }
    ctrl->SetSource(WrapSource(std::make_unique<FileSource>(text.c_str(), m_device)));

    auto* src = ctrl->GetSource();
    if (src) {
        SetWindowTextW(m_window, u2w(src->GetTitle()).c_str());
    }
}

// --- Capture ------------------------------------------------------------------

void Application::StartCapturePicking() {
    m_pickingMode = true;
    SetCapture(m_window);
    logger->info("Capture picking mode: click a window to capture");
}

Application::SnapTarget Application::ComputeSnapTarget(HWND hwnd, const RECT& windowRect) {
    const int threshold = m_previewVisible ? 157 : 147;

    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMonitor, &mi)) return {};

    RECT m = mi.rcMonitor;
    int winW = windowRect.right - windowRect.left;
    int winH = windowRect.bottom - windowRect.top;
    int monW = m.right - m.left;
    int monH = m.bottom - m.top;

    // Skip if window larger than monitor
    if (winW > monW || winH > monH) return {};

    int d_left   = windowRect.left - m.left;
    int d_right  = m.right - windowRect.right;
    int d_top    = windowRect.top - m.top;
    int d_bottom = m.bottom - windowRect.bottom;

    bool snapLeft   = d_left >= 0 && d_left <= threshold;
    bool snapRight  = d_right >= 0 && d_right <= threshold;
    bool snapTop    = d_top >= 0 && d_top <= threshold;
    bool snapBottom = d_bottom >= 0 && d_bottom <= threshold;

    if (!snapLeft && !snapRight && !snapTop && !snapBottom) return {};

    SnapTarget target;
    target.active = true;
    target.x = windowRect.left;
    target.y = windowRect.top;

    if (snapLeft)  target.x = m.left;
    if (snapRight) target.x = m.right - winW;
    if (snapTop)    target.y = m.top;
    if (snapBottom) target.y = m.bottom - winH;

    return target;
}

void Application::CreatePreviewWindow() {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {};
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"SnapPreview";
        wc.hbrBackground = CreateSolidBrush(RGB(0, 120, 215));
        wc.lpfnWndProc = DefWindowProc;
        RegisterClassW(&wc);
        registered = true;
    }
    m_hwndPreview = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        L"SnapPreview", L"",
        WS_POPUP,
        0, 0, 100, 100,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    SetLayeredWindowAttributes(m_hwndPreview, 0, 80, LWA_ALPHA);
}

void Application::UpdatePreviewWindow(int x, int y, int w, int h) {
    if (!m_hwndPreview) CreatePreviewWindow();
    SetWindowPos(m_hwndPreview, HWND_TOP, x, y, w, h,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    m_previewVisible = true;
}

void Application::HidePreviewWindow() {
    if (m_hwndPreview && m_previewVisible) {
        ShowWindow(m_hwndPreview, SW_HIDE);
        m_previewVisible = false;
    }
}

// --- Application -------------------------------------------------------------

Application::~Application() = default;

std::unique_ptr<IVideoSource> Application::WrapSource(std::unique_ptr<IVideoSource> inner) {
    auto proc = std::make_unique<VideoProcSource>(std::move(inner));
    auto filter = std::make_unique<GrayscaleFilter>();
    if (filter->Init(m_device)) {
        proc->AddFilter(std::move(filter));
        logger->info("WrapSource: GrayscaleFilter added");
    } else {
        logger->warn("WrapSource: GrayscaleFilter init FAILED — filter unavailable");
    }
    return proc;
}

void Application::OnIdle() {
    // Track key-up ourselves: RegisterHotKey without MOD_NOREPEAT delivers repeats
    // while the key is held, and a release is the only trustworthy "new gesture"
    // signal.
    if (!(GetAsyncKeyState(static_cast<int>(kCornerQuakeHotkeyVk)) & 0x8000)) {
        m_hotkeyReleased = true;
    }
    SyncHiddenStateWithWindow();
    m_controller->Render(m_window);
}

LRESULT Application::OnMessage(MSG& msg, bool& handled) {
    auto it = m_handlers.find(msg.message);
    if (it != m_handlers.end())
        return it->second(msg, handled);
    return 0;
}

void Application::InitHandlers() {
    m_handlers[WM_SIZE] = [this](MSG& m, bool& handled) -> LRESULT {
        // Minimizing reports a 0x0 client area; resizing the swapchain to that
        // would leave the backbuffer inconsistent for the next restore.
        if (m.wParam == SIZE_MINIMIZED) { handled = true; return 0; }
        if (m_controller) {
            auto width = GET_X_LPARAM(m.lParam);
            auto height = GET_Y_LPARAM(m.lParam);
            if (width > 0 && height > 0) {
                // The window is created as WS_POPUP | WS_VISIBLE, so its client area
                // is the whole window and no frame compensation applies. The old
                // branch subtracted a WS_OVERLAPPEDWINDOW frame whenever the style
                // contained WS_CLIPSIBLINGS - a bit this process never sets - so it
                // could not run; adding a framed window mode later has to opt in
                // here instead of relying on that condition.
                m_controller->ResizeSwapChain(width, height);
            }
        }
        handled = true; return 0;
    };

    m_handlers[WM_NCHITTEST] = [](MSG& m, bool& handled) -> LRESULT {
        if (GetKeyState(VK_MENU) & 0x8000) {
            RECT r; GetWindowRect(m.hwnd, &r);
            long x = GET_X_LPARAM(m.lParam), y = GET_Y_LPARAM(m.lParam);
            const int bw = 8;
            handled = true;
            if (x >= r.left && x < r.left + bw && y >= r.top && y < r.top + bw) return HTTOPLEFT;
            if (x < r.right && x >= r.right - bw && y >= r.top && y < r.top + bw) return HTTOPRIGHT;
            if (x >= r.left && x < r.left + bw && y < r.bottom && y >= r.bottom - bw) return HTBOTTOMLEFT;
            if (x < r.right && x >= r.right - bw && y < r.bottom && y >= r.bottom - bw) return HTBOTTOMRIGHT;
            if (x >= r.left && x < r.left + bw) return HTLEFT;
            if (x < r.right && x >= r.right - bw) return HTRIGHT;
            if (y >= r.top && y < r.top + bw) return HTTOP;
            if (y < r.bottom && y >= r.bottom - bw) return HTBOTTOM;
            return HTCAPTION;
        }
        return 0;
    };

    m_handlers[WM_LBUTTONDOWN] = [this](MSG& m, bool& handled) -> LRESULT {
        if (m_pickingMode) {
            ReleaseCapture();
            m_pickingMode = false;

            POINT pt = { GET_X_LPARAM(m.lParam), GET_Y_LPARAM(m.lParam) };
            ClientToScreen(m.hwnd, &pt);
            HWND targetHwnd = WindowFromPoint(pt);
            if (targetHwnd && targetHwnd != m.hwnd) {
                logger->info("Capture: attempting to capture HWND=0x{:X}", (uint64_t)targetHwnd);

                auto captureSource = std::make_unique<CaptureSource>(targetHwnd, m_device);
                m_controller->SetSource(WrapSource(std::move(captureSource)));
                auto* src = m_controller->GetSource();
                if (src) {
                    auto* vq = m_controller->GetVideoQuad();
                    vq->InitCapture(src->GetWidth(), src->GetHeight());
                    SetWindowTextW(m.hwnd, (L"Capturing: " + u2w(src->GetTitle())).c_str());
                    logger->info("Capture started: {}x{}", src->GetWidth(), src->GetHeight());
                } else {
                    SetWindowTextW(m.hwnd, L"D3D Video");
                }
            } else {
                logger->info("Capture: target window is self or null (0x{:X})", (uint64_t)targetHwnd);
            }
        }
        return 0;
    };

    m_handlers[WM_KEYDOWN] = [this](MSG& m, bool& handled) -> LRESULT {
        if (m.wParam == VK_ESCAPE) {
            if (m_pickingMode) {
                ReleaseCapture();
                m_pickingMode = false;
                logger->info("ESC: capture picking cancelled");
            } else if (m_controller && m_controller->GetSource()) {
                logger->info("ESC: stopping source");
                m_controller->StopSource();
                SetWindowTextW(m.hwnd, L"D3D Video");
            }
        }
        if (m.wParam == 0x43) {
            logger->info("C key: starting capture picking");
            StartCapturePicking();
            handled = true; return 0;
        }
        if (m.wParam == 0x46) {
            if (m_controller && m_controller->GetSource()) {
                auto* proc = dynamic_cast<VideoProcSource*>(m_controller->GetSource());
                if (proc) {
                    bool wasOn = proc->IsFilterEnabled();
                    proc->ToggleFilter();
                    logger->info("F key: filter toggled {} -> {}", wasOn, proc->IsFilterEnabled());
                } else {
                    logger->info("F key: source is not VideoProcSource (no filter)");
                }
            } else {
                logger->info("F key: no source loaded");
            }
            handled = true; return 0;
        }
        if (m.wParam == VK_F11) {
            g_isFullscreen = !g_isFullscreen;
            if (g_isFullscreen) {
                GetWindowRect(m.hwnd, &g_windowedRect);
                HMONITOR hMonitor = MonitorFromWindow(m.hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi;
                mi.cbSize = sizeof(MONITORINFO);
                GetMonitorInfo(hMonitor, &mi);
                SetWindowPos(m.hwnd, HWND_TOP,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            } else {
                SetWindowPos(m.hwnd, HWND_NOTOPMOST,
                    g_windowedRect.left, g_windowedRect.top,
                    g_windowedRect.right - g_windowedRect.left,
                    g_windowedRect.bottom - g_windowedRect.top,
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            }
        }
        handled = true; return 0;
    };

    m_handlers[WM_HOTKEY] = [this](MSG& m, bool& handled) -> LRESULT {
        if (m.wParam == kCornerQuakeHotkeyId) {
            ToggleCornerQuake();
        }
        handled = true; return 0;
    };

    m_handlers[WM_POWERBROADCAST] = [this](MSG& m, bool& handled) -> LRESULT {
        if (m.wParam == PBT_APMSUSPEND && m_controller) {
            m_controller->OnSystemSuspend();
        } else if (m.wParam == PBT_APMRESUMEAUTOMATIC && m_controller) {
            m_controller->OnSystemResume();
        }
        handled = true; return TRUE;
    };

    m_handlers[WM_KEYUP] = [](MSG&, bool& handled) -> LRESULT {
        handled = true; return 0;
    };

    m_handlers[WM_ENTERSIZEMOVE] = [this](MSG& m, bool& handled) -> LRESULT {
        if (m_controller) m_controller->Pause(PauseReason::WindowDrag);
        if (g_isFullscreen || !(GetKeyState(VK_MENU) & 0x8000)) return 0;
        return 0;
    };

    m_handlers[WM_MOVING] = [this](MSG& m, bool& handled) -> LRESULT {
        if (g_isFullscreen || !(GetKeyState(VK_MENU) & 0x8000)) return 0;
        m_moveActive = true;
        RECT* pr = reinterpret_cast<RECT*>(m.lParam);
        auto target = ComputeSnapTarget(m.hwnd, *pr);
        if (target.active) {
            int w = pr->right - pr->left;
            int h = pr->bottom - pr->top;
            UpdatePreviewWindow(target.x, target.y, w, h);
        } else {
            HidePreviewWindow();
        }
        return 0;
    };

    m_handlers[WM_EXITSIZEMOVE] = [this](MSG& m, bool& handled) -> LRESULT {
        if (m_controller) m_controller->Resume(PauseReason::WindowDrag);
        if (g_isFullscreen || !m_moveActive) return 0;
        m_moveActive = false;

        RECT wr;
        GetWindowRect(m.hwnd, &wr);
        // Compute snap while m_previewVisible still reflects drag state,
        // so hysteresis threshold is used if preview was showing.
        auto target = ComputeSnapTarget(m.hwnd, wr);
        HidePreviewWindow();

        if (target.active) {
            SetWindowPos(m.hwnd, nullptr, target.x, target.y, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        handled = false; return 0;
    };

    m_handlers[WM_DESTROY] = [this](MSG&, bool& handled) -> LRESULT {
        if (m_hwndPreview) {
            DestroyWindow(m_hwndPreview);
            m_hwndPreview = nullptr;
        }
        PostQuitMessage(0);
        handled = true; return 0;
    };

    m_handlers[WM_NCLBUTTONDBLCLK] = [this](MSG& m, bool& handled) -> LRESULT {
        // Alt + double-click window edge → fit window to video aspect ratio.
        // The clicked edge stays fixed (position + length), opposite edge moves.
        if (!(GetKeyState(VK_MENU) & 0x8000)) return 0;
        auto* src = m_controller ? m_controller->GetSource() : nullptr;
        if (!src) return 0;

        int vw = src->GetWidth();
        int vh = src->GetHeight();
        if (vw <= 0 || vh <= 0) return 0;
        double videoRatio = (double)vw / vh;

        UINT edge = static_cast<UINT>(m.wParam);
        RECT wr;
        GetWindowRect(m.hwnd, &wr);
        int cx = wr.left, cy = wr.top, cw = wr.right - wr.left, ch = wr.bottom - wr.top;
        int newW, newH, newX, newY;

        if (edge == HTTOP || edge == HTBOTTOM) {
            // Horizontal edge clicked → width fixed, adjust height
            newW = cw;
            newH = static_cast<int>(cw / videoRatio);
            newX = cx;
            newY = (edge == HTTOP) ? cy : cy + ch - newH;
        } else if (edge == HTLEFT || edge == HTRIGHT) {
            // Vertical edge clicked → height fixed, adjust width
            newH = ch;
            newW = static_cast<int>(ch * videoRatio);
            newX = (edge == HTLEFT) ? cx : cx + cw - newW;
            newY = cy;
        } else {
            return 0;
        }
        SetWindowPos(m.hwnd, nullptr, newX, newY, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE);
        handled = true; return 0;
    };
}

// --- Alt+Esc corner quake ----------------------------------------------------

void Application::ToggleCornerQuake() {
    // Capture picking waits for a click on a target window; collapsing the player
    // mid-gesture would leave SetCapture dangling.
    if (m_pickingMode) {
        logger->info("Alt+Esc ignored: capture picking in progress");
        return;
    }

    // The same press can arrive more than once, and holding the combination makes
    // Windows repeat it: treat anything inside the duplicate window as one gesture,
    // and while the key still reads as held treat it as auto-repeat -- but only for
    // the grace window, so a keyboard that never sends a key-up cannot lock the
    // toggle out.
    const auto now = std::chrono::steady_clock::now();
    // Note: windows.h defines max()/min() as macros, so avoid std::chrono::...::max().
    const bool haveLast = m_lastToggle.time_since_epoch().count() != 0;
    const auto sinceLast = haveLast
        ? std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastToggle)
        : std::chrono::milliseconds(0);
    if (haveLast && sinceLast < std::chrono::milliseconds(kCornerQuakeDuplicateMs)) {
        logger->info("Alt+Esc ignored: duplicate within {} ms", kCornerQuakeDuplicateMs);
        return;
    }
    if (haveLast && !m_hotkeyReleased &&
        sinceLast < std::chrono::milliseconds(kCornerQuakeRepeatGraceMs)) {
        logger->info("Alt+Esc ignored: key still held (auto-repeat)");
        return;
    }
    m_lastToggle = now;
    m_hotkeyReleased = false;

    if (m_hidden) {
        ShowInCorner();
    } else {
        HideFromCorner();
    }
}

void Application::HideFromCorner() {
    if (!m_window) return;

    // A fullscreen window cannot be anchored in a corner, and silently ignoring
    // the hotkey while fullscreen is more confusing than leaving fullscreen.
    if (g_isFullscreen) {
        g_isFullscreen = false;
        RECT r = g_windowedRect;
        SetWindowPos(m_window, HWND_NOTOPMOST, r.left, r.top,
                     r.right - r.left, r.bottom - r.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        logger->info("Alt+Esc: left fullscreen before collapsing");
    }

    m_hidden = true;
    if (m_controller) m_controller->SetHidden(true);

    ParkOffscreen();
    logger->info("Alt+Esc: player collapsed (parked off screen, no taskbar, no Alt+Tab entry)");
}

void Application::ParkOffscreen() {
    // WS_EX_TOOLWINDOW removes the taskbar button and the Alt+Tab entry; the
    // off-screen position makes it invisible without clearing WS_VISIBLE.
    LONG_PTR exStyle = GetWindowLongPtr(m_window, GWL_EXSTYLE);
    SetWindowLongPtr(m_window, GWL_EXSTYLE, exStyle | WS_EX_TOOLWINDOW);
    SetWindowPos(m_window, nullptr, kCornerQuakeParkX, kCornerQuakeParkY, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    m_collapsedParked = true;
}

void Application::UnparkFromOffscreen() {
    if (!m_collapsedParked) return;
    LONG_PTR exStyle = GetWindowLongPtr(m_window, GWL_EXSTYLE);
    SetWindowLongPtr(m_window, GWL_EXSTYLE, exStyle & ~WS_EX_TOOLWINDOW);
    m_collapsedParked = false;
}

void Application::ShowInCorner() {
    if (!m_window) return;

    // The collapsed player is normally parked off screen while still visible; a
    // window minimized by other means needs a restore instead.
    UnparkFromOffscreen();
    ShowWindow(m_window, IsIconic(m_window) ? SW_RESTORE : SW_SHOW);
    MoveToCursorMonitorCorner();

    m_hidden = false;
    if (m_controller) m_controller->SetHidden(false);
    SetForegroundWindow(m_window);
    logger->info("Alt+Esc: player summoned");
}

bool Application::MoveToCursorMonitorCorner() {
    POINT pt = {};
    if (!GetCursorPos(&pt)) {
        logger->warn("Alt+Esc: GetCursorPos failed, keeping current position");
        return false;
    }

    HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMonitor, &mi)) {
        logger->warn("Alt+Esc: GetMonitorInfo failed, keeping current position");
        return false;
    }

    // Keep the current size and only anchor the top-left corner, using the work
    // area so a taskbar docked left or top cannot cover the window. The frame
    // change flushes the WS_EX_TOOLWINDOW removal done by UnparkFromOffscreen().
    SetWindowPos(m_window, nullptr, mi.rcWork.left, mi.rcWork.top, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    logger->info("Alt+Esc: anchored to ({}, {})", mi.rcWork.left, mi.rcWork.top);
    return true;
}

void Application::SyncHiddenStateWithWindow() {
    if (!m_window) return;

    // While parked, keep the window out of the minimized state: minimizing also
    // stops hotkey delivery, and a parked window has no taskbar entry to recover
    // from. Re-parking is visually a no-op because it is off screen either way.
    if (m_collapsedParked && IsIconic(m_window)) {
        ShowWindow(m_window, SW_RESTORE);
        SetWindowPos(m_window, nullptr, kCornerQuakeParkX, kCornerQuakeParkY, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        logger->warn("Window minimized while collapsed; re-parked to keep the hotkey reachable");
    }

    // "Off screen" covers every collapse style: parked (no monitor intersects the
    // window) plus hidden or minimized by other means such as Win+D.
    const bool offscreen = IsIconic(m_window) != FALSE ||
                           !IsWindowVisible(m_window) ||
                           MonitorFromWindow(m_window, MONITOR_DEFAULTTONULL) == nullptr;
    if (offscreen == m_hidden) return;

    m_hidden = offscreen;
    if (m_controller) m_controller->SetHidden(offscreen);
    logger->info(offscreen ? "Window off screen outside the hotkey: rendering suspended"
                           : "Window back on screen outside the hotkey: rendering resumed");
}

int Application::Run(HINSTANCE hInstance) {
    InitCrashHandler();
    SetProcessDPIAware();
    InitLogger();

    g_app = this;

    auto className = L"MyWindow";
    WNDCLASSW wndClass = {};
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = className;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.lpfnWndProc = Application::WndProc;

    RegisterClass(&wndClass);
    m_window = CreateWindow(className, L"D3D Video",
        WS_POPUP | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL);

    ImmAssociateContext(m_window, NULL);

    OleInitialize(NULL);
    HRESULT hrRoInit = RoInitialize(RO_INIT_SINGLETHREADED);
    logger->info("RoInitialize result: 0x{:08X}", (uint32_t)hrRoInit);
    HRESULT hrDrag = RegisterDragDrop(m_window, static_cast<IDropTarget*>(this));
    if (FAILED(hrDrag)) {
        logger->warn("RegisterDragDrop failed: 0x{:08X}, drag-drop disabled", (uint32_t)hrDrag);
    }

    ShowWindow(m_window, SW_SHOW);
    SetForegroundWindow(m_window);

    if (!InitD3D11(m_window)) {
        RevokeDragDrop(m_window);
        RoUninitialize();
        OleUninitialize();
        return -1;
    }

    RECT clientRect;
    GetClientRect(m_window, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    m_controller = std::make_unique<VideoController>();
    m_controller->Init(m_device, m_deviceCtx, m_swapChain, clientWidth, clientHeight);

    InitHandlers();

    m_powerNotify = RegisterSuspendResumeNotification(m_window, DEVICE_NOTIFY_WINDOW_HANDLE);

    // Deliberately no MOD_NOREPEAT: its suppression logic depends on seeing a
    // key-up, so a keyboard that only reports key-down gets exactly one delivery
    // ever (measured 1/3 versus 3/3 without it) and the collapsed window becomes
    // unreachable. Repeat handling lives in ToggleCornerQuake() instead.
    m_hotkeyRegistered = RegisterHotKey(NULL, kCornerQuakeHotkeyId,
                                        kCornerQuakeModKey, kCornerQuakeHotkeyVk) != FALSE;
    if (m_hotkeyRegistered) {
        logger->info("RegisterHotKey(Alt+Esc) ok: the hotkey toggles the corner quake from any app");
    } else {
        logger->error("RegisterHotKey(Alt+Esc) failed: 0x{:08X}", (uint32_t)GetLastError());
    }

    MessageLoop loop;
    int exitCode = loop.Run(m_window, static_cast<MessageLoop::ICallback*>(this));

    if (m_powerNotify) {
        UnregisterSuspendResumeNotification(m_powerNotify);
        m_powerNotify = nullptr;
    }
    if (m_hotkeyRegistered) {
        UnregisterHotKey(NULL, kCornerQuakeHotkeyId);
        m_hotkeyRegistered = false;
    }
    RevokeDragDrop(m_window);
    RoUninitialize();
    OleUninitialize();
    m_controller.reset();
    if (m_deviceCtx) m_deviceCtx->Release();
    if (m_swapChain) m_swapChain->Release();
    if (m_device) m_device->Release();
    ShutdownCrashHandler();
    return exitCode;
}

bool Application::InitD3D11(HWND window) {
    RECT clientRect;
    GetClientRect(window, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    auto& bufferDesc = swapChainDesc.BufferDesc;
    bufferDesc.Width = clientWidth;
    bufferDesc.Height = clientHeight;
    bufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    bufferDesc.RefreshRate.Numerator = 0;
    bufferDesc.RefreshRate.Denominator = 0;
    bufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
    bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.OutputWindow = window;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.Flags = 0;

    UINT flags = 0;
#ifdef DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    flags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    D3D_FEATURE_LEVEL level;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags,
        NULL, NULL, D3D11_SDK_VERSION, &swapChainDesc,
        &m_swapChain, &m_device, &level, &m_deviceCtx);

    if (FAILED(hr)) {
        logger->error("D3D11CreateDeviceAndSwapChain failed: 0x{:08X}", (uint32_t)hr);
    }
    return SUCCEEDED(hr);
}

LRESULT CALLBACK Application::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!g_app) return DefWindowProc(hwnd, msg, wParam, lParam);
    MSG m = { hwnd, msg, wParam, lParam };
    bool handled = false;
    LRESULT result = g_app->OnMessage(m, handled);
    return handled ? result : DefWindowProc(hwnd, msg, wParam, lParam);
}
