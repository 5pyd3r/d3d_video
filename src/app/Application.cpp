#include "Application.h"
#include "../platform/Logger.h"
#include "../platform/StringUtils.h"
#include "../platform/CrashHandler.h"
#include "../platform/StreamUtils.h"
#include "../platform/ScreenCapture.h"
#include "../playback/PlaybackController.h"

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
    // Loading a video replaces the current capture
    if (m_captureState == CaptureState::Capturing ||
        m_captureState == CaptureState::Picking || m_captureFrozen) {
        m_captureFrozen = false;
        StopCurrentCapture();
    }

    FORMATETC fmt = {};
    fmt.cfFormat = CF_HDROP;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;

    STGMEDIUM medium = {};
    if (FAILED(pDataObj->GetData(&fmt, &medium))) return;

    HDROP hDrop = (HDROP)medium.hGlobal;
    wchar_t filePath[MAX_PATH];
    DragQueryFile(hDrop, 0, filePath, MAX_PATH);

    auto* pc = m_playback.get();
    if (!pc) { ReleaseStgMedium(&medium); return; }

    DWORD attrs = GetFileAttributesW(filePath);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        std::vector<std::string> playlist;
        std::function<void(const std::wstring&)> enumerate =
            [&](const std::wstring& dir) {
            std::wstring searchPath = dir + L"\\*";
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) return;
            do {
                if (wcscmp(fd.cFileName, L".") == 0 ||
                    wcscmp(fd.cFileName, L"..") == 0) continue;
                std::wstring fullPath = dir + L"\\" + fd.cFileName;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    enumerate(fullPath);
                } else {
                    std::string utf8Path = w2u(fullPath);
                    if (IsVideoFile(utf8Path)) {
                        playlist.push_back(utf8Path);
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        };
        enumerate(std::wstring(filePath));
        if (!playlist.empty()) {
            std::sort(playlist.begin(), playlist.end());
            pc->SetPlaylist(playlist);
            SetWindowTextW(m_window, TruncateFileNameForTitle(pc->GetCurrentFilePath()).c_str());
        }
    } else {
        std::string utf8Path = w2u(filePath);
        uint32_t ret = pc->LoadFile(utf8Path.c_str());
        if (ret != 0) {
            logger->error("LoadFile failed: {}", ret);
        } else {
            SetWindowTextW(m_window, TruncateFileNameForTitle(utf8Path).c_str());
        }
    }
    ReleaseStgMedium(&medium);
}

void Application::HandleTextDrop(IDataObject* pDataObj) {
    // Loading a stream/video replaces the current capture
    if (m_captureState == CaptureState::Capturing ||
        m_captureState == CaptureState::Picking || m_captureFrozen) {
        m_captureFrozen = false;
        StopCurrentCapture();
    }

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

    auto* pc = m_playback.get();
    if (!pc) return;

    if (IsStreamUri(text)) {
        uint32_t ret = pc->LoadFile(text.c_str());
        if (ret != 0) {
            logger->error("LoadFile (URI) failed: {}", ret);
        } else {
            SetWindowTextW(m_window, TruncateFileNameForTitle(text).c_str());
        }
    } else {
        // Treat as a file path (for text-dropped absolute paths)
        uint32_t ret = pc->LoadFile(text.c_str());
        if (ret != 0) {
            logger->error("LoadFile failed: {}", ret);
        } else {
            SetWindowTextW(m_window, TruncateFileNameForTitle(text).c_str());
        }
    }
}

// --- Capture ------------------------------------------------------------------

void Application::StartCapturePicking() {
    m_captureState = CaptureState::Picking;
    SetCapture(m_window);
    logger->info("Capture picking mode: click a window to capture");
}

void Application::StopCurrentCapture() {
    if (m_capture) {
        m_capture->StopCapture();
    }
    m_captureTarget = nullptr;
    m_captureState = CaptureState::Idle;
    SetWindowTextW(m_window, L"D3D Video");
}

void Application::RenderCapture() {
    if (!m_capture) return;

    auto* vq = m_playback->GetVideoQuad();
    auto& scm = m_playback->GetSwapChainMgr();

    int width = m_capture->GetWidth();
    int height = m_capture->GetHeight();
    if (width <= 0 || height <= 0) return;

    // BeginFrame must come first to set the render target (matches PlaybackController::Draw order)
    scm.BeginFrame();
    vq->BeginDraw();
    RECT rect;
    GetClientRect(m_window, &rect);
    double srcRatio = (double)width / height;
    double dstRatio = (double)rect.right / rect.bottom;
    vq->UpdateByRatio(srcRatio, dstRatio);

    // Only fetch new frames while capture is active; frozen mode re-renders last texture
    if (m_capture->IsCapturing()) {
        m_capture->ProcessFrame(m_deviceCtx, vq, width, height);
    }

    vq->DrawCapture();
    scm.EndFrame();
    ClipCursor(NULL);
}

// --- Application -------------------------------------------------------------

Application::~Application() = default;

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

    m_playback = std::make_unique<PlaybackController>();
    m_playback->Init(m_device, m_deviceCtx, m_swapChain, clientWidth, clientHeight);
    SetWindowLongPtr(m_window, GWLP_USERDATA, (LONG_PTR)m_playback.get());

    MSG msg;
    while (1) {
        BOOL hasMsg = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
        if (hasMsg) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else if (m_captureState == CaptureState::Capturing || m_captureFrozen) {
            // Detect if captured window was closed
            if (m_captureTarget && !IsWindow(m_captureTarget)) {
                logger->info("Capture target window closed, stopping capture");
                m_captureFrozen = true;
                StopCurrentCapture();
            } else {
                RenderCapture();
            }
        } else {
            m_playback->Render(m_window);
        }
    }

    RevokeDragDrop(m_window);
    RoUninitialize();
    OleUninitialize();
    m_playback.reset();
    if (m_deviceCtx) m_deviceCtx->Release();
    if (m_swapChain) m_swapChain->Release();
    if (m_device) m_device->Release();
    ShutdownCrashHandler();
    return 0;
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
    switch (msg) {
    case WM_SIZE: {
        auto* pc = (PlaybackController*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (pc) {
            auto width = GET_X_LPARAM(lParam);
            auto height = GET_Y_LPARAM(lParam);
            if ((GetWindowLongPtr(hwnd, GWL_STYLE) & (WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS)) == (WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS)) {
                RECT cr = {0, 0, 100, 100};
                AdjustWindowRect(&cr, WS_OVERLAPPEDWINDOW, FALSE);
                width = width - (cr.right - cr.left - 100);
                height = height - (cr.bottom - cr.top - 100);
            }
            pc->ResizeSwapChain(width, height);
        }
        return 0;
    }
    case WM_NCHITTEST: {
        if (GetKeyState(VK_MENU) & 0x8000) {
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);
            long x = GET_X_LPARAM(lParam);
            long y = GET_Y_LPARAM(lParam);
            const int bw = 8;
            if (x >= windowRect.left && x < windowRect.left + bw && y >= windowRect.top && y < windowRect.top + bw) return HTTOPLEFT;
            if (x < windowRect.right && x >= windowRect.right - bw && y >= windowRect.top && y < windowRect.top + bw) return HTTOPRIGHT;
            if (x >= windowRect.left && x < windowRect.left + bw && y < windowRect.bottom && y >= windowRect.bottom - bw) return HTBOTTOMLEFT;
            if (x < windowRect.right && x >= windowRect.right - bw && y < windowRect.bottom && y >= windowRect.bottom - bw) return HTBOTTOMRIGHT;
            if (x >= windowRect.left && x < windowRect.left + bw) return HTLEFT;
            if (x < windowRect.right && x >= windowRect.right - bw) return HTRIGHT;
            if (y >= windowRect.top && y < windowRect.top + bw) return HTTOP;
            if (y < windowRect.bottom && y >= windowRect.bottom - bw) return HTBOTTOM;
            return HTCAPTION;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    case WM_LBUTTONDOWN:
        if (g_app && g_app->m_captureState == CaptureState::Picking) {
            ReleaseCapture();
            
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hwnd, &pt);
            HWND targetHwnd = WindowFromPoint(pt);
            if (targetHwnd && targetHwnd != hwnd) {
                if (!g_app->m_capture) {
                    g_app->m_capture = std::make_unique<ScreenCapture>();
                }
                logger->info("Capture: attempting to capture HWND=0x{:X}", (uint64_t)targetHwnd);
                if (g_app->m_capture->StartCapture(targetHwnd, g_app->m_device)) {
                    g_app->m_captureState = CaptureState::Capturing;
                    g_app->m_captureFrozen = false;
                    g_app->m_captureTarget = targetHwnd;
                    auto* vq = g_app->m_playback->GetVideoQuad();
                    vq->InitCapture(g_app->m_capture->GetWidth(), g_app->m_capture->GetHeight());
                    wchar_t title[256];
                    GetWindowTextW(targetHwnd, title, 256);
                    SetWindowTextW(hwnd, (L"Capturing: " + std::wstring(title)).c_str());
                    logger->info("Capture started: {}x{}, title='{}'", g_app->m_capture->GetWidth(), g_app->m_capture->GetHeight(), w2u(title));
                } else {
                    g_app->m_captureState = CaptureState::Idle;
                    g_app->m_captureFrozen = false;
                    g_app->m_capture.reset();
                    SetWindowTextW(hwnd, L"D3D Video");
                }
            } else {
                logger->info("Capture: target window is self or null (0x{:X})", (uint64_t)targetHwnd);
                g_app->m_captureState = CaptureState::Idle;
            }
            break;
        }
        break;
    case WM_KEYDOWN:
        // Capture hotkeys
        if (wParam == VK_ESCAPE) {
            if (g_app && g_app->m_captureState == CaptureState::Capturing) {
                g_app->m_captureFrozen = true;
                g_app->StopCurrentCapture();
            } else if (g_app && g_app->m_captureState == CaptureState::Picking) {
                ReleaseCapture();
                
                // If we entered picking from an active capture, restore it
                if (g_app->m_capture && g_app->m_capture->IsCapturing()) {
                    g_app->m_captureState = CaptureState::Capturing;
                } else {
                    g_app->m_captureState = CaptureState::Idle;
                }
            }
        }
        if (wParam == 0x43 && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
            if (g_app) {
                if (g_app->m_captureState == CaptureState::Capturing) {
                    // Directly enter picking mode without stopping current capture.
                    // Selecting a new window will replace the capture automatically.
                    g_app->StartCapturePicking();
                } else if (g_app->m_captureState == CaptureState::Idle) {
                    g_app->StartCapturePicking();
                }
            }
            break;
        }
        if (wParam == VK_F11) {
            g_isFullscreen = !g_isFullscreen;
            if (g_isFullscreen) {
                GetWindowRect(hwnd, &g_windowedRect);
                HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi;
                mi.cbSize = sizeof(MONITORINFO);
                GetMonitorInfo(hMonitor, &mi);
                SetWindowPos(hwnd, HWND_TOP,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            } else {
                SetWindowPos(hwnd, HWND_NOTOPMOST,
                    g_windowedRect.left, g_windowedRect.top,
                    g_windowedRect.right - g_windowedRect.left,
                    g_windowedRect.bottom - g_windowedRect.top,
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            }
        }
        break;
    case WM_KEYUP:
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
