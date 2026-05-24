#include "Application.h"
#include "../platform/Logger.h"
#include "../platform/StringUtils.h"
#include "../platform/CrashHandler.h"
#include "../playback/PlaybackController.h"

#include <windowsx.h>
#include <ShlObj.h>
#include <vector>
#include <algorithm>
#include <functional>

#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600

static bool g_isFullscreen = false;
static RECT g_windowedRect;

Application::~Application() = default;

int Application::Run(HINSTANCE hInstance) {
    InitCrashHandler();
    CoInitializeEx(NULL, COINIT::COINIT_MULTITHREADED);
    SetProcessDPIAware();
    InitLogger();

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

    DragAcceptFiles(m_window, TRUE);
    ShowWindow(m_window, SW_SHOW);
    SetForegroundWindow(m_window);

    if (!InitD3D11(m_window)) {
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
        } else {
            m_playback->Render(m_window);
        }
    }

    m_playback.reset();
    if (m_deviceCtx) m_deviceCtx->Release();
    if (m_swapChain) m_swapChain->Release();
    if (m_device) m_device->Release();
    ShutdownCrashHandler();
    CoUninitialize();
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
    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        wchar_t filePath[MAX_PATH];
        DragQueryFile(hDrop, 0, filePath, MAX_PATH);

        auto* pc = (PlaybackController*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!pc) { DragFinish(hDrop); return 0; }

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
                SetWindowTextW(hwnd, TruncateFileNameForTitle(pc->GetCurrentFilePath()).c_str());
            }
        } else {
            std::string utf8Path = w2u(filePath);
            uint32_t ret = pc->LoadFile(utf8Path.c_str());
            if (ret != 0) {
                logger->error("LoadFile failed: {}", ret);
            } else {
                SetWindowTextW(hwnd, TruncateFileNameForTitle(utf8Path).c_str());
            }
        }
        DragFinish(hDrop);
        return 0;
    }
    case WM_KEYDOWN:
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
