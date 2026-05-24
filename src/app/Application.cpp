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
    wchar_t filePath[MAX_PATH];
    DragQueryFile(hDrop, 0, filePath, MAX_PATH);

    auto* ctrl = m_controller.get();
    if (!ctrl) { ReleaseStgMedium(&medium); return; }

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
            ctrl->SetSource(std::make_unique<PlaylistSource>(playlist, m_device));
        }
    } else {
        std::string utf8Path = w2u(filePath);
        ctrl->SetSource(std::make_unique<FileSource>(utf8Path.c_str(), m_device));
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

    m_controller = std::make_unique<VideoController>();
    m_controller->Init(m_device, m_deviceCtx, m_swapChain, clientWidth, clientHeight);

    MSG msg;
    while (1) {
        BOOL hasMsg = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
        if (hasMsg) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            m_controller->Render(m_window);
        }
    }

    RevokeDragDrop(m_window);
    RoUninitialize();
    OleUninitialize();
    m_controller.reset();
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
        auto* app = g_app;
        if (app && app->m_controller) {
            auto width = GET_X_LPARAM(lParam);
            auto height = GET_Y_LPARAM(lParam);
            if ((GetWindowLongPtr(hwnd, GWL_STYLE) & (WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS)) == (WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS)) {
                RECT cr = {0, 0, 100, 100};
                AdjustWindowRect(&cr, WS_OVERLAPPEDWINDOW, FALSE);
                width = width - (cr.right - cr.left - 100);
                height = height - (cr.bottom - cr.top - 100);
            }
            app->m_controller->ResizeSwapChain(width, height);
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
        if (g_app && g_app->m_pickingMode) {
            ReleaseCapture();
            g_app->m_pickingMode = false;

            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hwnd, &pt);
            HWND targetHwnd = WindowFromPoint(pt);
            if (targetHwnd && targetHwnd != hwnd) {
                logger->info("Capture: attempting to capture HWND=0x{:X}", (uint64_t)targetHwnd);

                auto captureSource = std::make_unique<CaptureSource>(targetHwnd, g_app->m_device);

                g_app->m_controller->SetSource(std::move(captureSource));
                auto* src = g_app->m_controller->GetSource();
                if (src) {
                    // Must call InitCapture AFTER SetSource so we know the actual capture size.
                    // If the capture texture size doesn't match the frame pool texture size,
                    // CopySubresourceRegion in ProcessFrame would produce garbled output.
                    auto* vq = g_app->m_controller->GetVideoQuad();
                    vq->InitCapture(src->GetWidth(), src->GetHeight());
                    SetWindowTextW(hwnd, (L"Capturing: " + u2w(src->GetTitle())).c_str());
                    logger->info("Capture started: {}x{}", src->GetWidth(), src->GetHeight());
                } else {
                    SetWindowTextW(hwnd, L"D3D Video");
                }
            } else {
                logger->info("Capture: target window is self or null (0x{:X})", (uint64_t)targetHwnd);
            }
            break;
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (g_app && g_app->m_pickingMode) {
                ReleaseCapture();
                g_app->m_pickingMode = false;
            } else if (g_app && g_app->m_controller && g_app->m_controller->GetSource()) {
                g_app->m_controller->StopSource();
                SetWindowTextW(hwnd, L"D3D Video");
            }
        }
        if (wParam == 0x43 && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
            if (g_app) {
                g_app->StartCapturePicking();
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
