#include "Application.h"
#include "../platform/Logger.h"
#include "../platform/StringUtils.h"
#include "../platform/CrashHandler.h"
#include "../platform/StreamUtils.h"
#include "../source/FileSource.h"
#include "../source/CaptureSource.h"
#include "../source/PlaylistSource.h"

#include <windowsx.h>
#include <ShlObj.h>
#include <roapi.h>
#include <vector>
#include <algorithm>
#include <functional>

#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600

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
    ULONG count = InterlockedDecrement(&m_refCount);
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
            ctrl->SetSource(std::make_unique<FileSource>(playlist[0].c_str(), m_device));
        } else {
            ctrl->SetSource(std::make_unique<PlaylistSource>(playlist, m_device));
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

    if (IsStreamUri(text)) {
        ctrl->SetSource(std::make_unique<FileSource>(text.c_str(), m_device));
    } else {
        ctrl->SetSource(std::make_unique<FileSource>(text.c_str(), m_device));
    }

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

void Application::OnIdle() {
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
        if (m_controller) {
            auto width = GET_X_LPARAM(m.lParam);
            auto height = GET_Y_LPARAM(m.lParam);
            if ((GetWindowLongPtr(m.hwnd, GWL_STYLE) & (WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS)) == (WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS)) {
                RECT cr = {0, 0, 100, 100};
                AdjustWindowRect(&cr, WS_OVERLAPPEDWINDOW, FALSE);
                width = width - (cr.right - cr.left - 100);
                height = height - (cr.bottom - cr.top - 100);
            }
            m_controller->ResizeSwapChain(width, height);
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
                m_controller->SetSource(std::move(captureSource));
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
            } else if (m_controller && m_controller->GetSource()) {
                m_controller->StopSource();
                SetWindowTextW(m.hwnd, L"D3D Video");
            }
        }
        if (m.wParam == 0x43) {
            StartCapturePicking();
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

    m_handlers[WM_KEYUP] = [](MSG&, bool& handled) -> LRESULT {
        handled = true; return 0;
    };

    m_handlers[WM_ENTERSIZEMOVE] = [this](MSG& m, bool& handled) -> LRESULT {
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

    MessageLoop loop;
    int exitCode = loop.Run(m_window, static_cast<MessageLoop::ICallback*>(this));

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

    return SUCCEEDED(hr);
}

LRESULT CALLBACK Application::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!g_app) return DefWindowProc(hwnd, msg, wParam, lParam);
    MSG m = { hwnd, msg, wParam, lParam };
    bool handled = false;
    LRESULT result = g_app->OnMessage(m, handled);
    return handled ? result : DefWindowProc(hwnd, msg, wParam, lParam);
}
