#define NOMINMAX

#include <stdio.h>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <memory>
#include <cmath>
#include <limits>
#include <map>

#include <Windows.h>
#include <windowsx.h>
#include <ShlObj.h>
#include <wrl.h>


#include <d3d11.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>

#include "render/VideoQuad.h"
#include "RenderParam.h"
#include "platform/Logger.h"
#include "platform/StringUtils.h"
#include "platform/CrashHandler.h"

using namespace std::chrono;
namespace dx = DirectX;

using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::wstring;







#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600

static bool g_isFullscreen = false;
static RECT g_windowedRect;

int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE,
	_In_ LPSTR,
	_In_ int)
{
	InitCrashHandler();

	CoInitializeEx(NULL, COINIT::COINIT_MULTITHREADED);
	SetProcessDPIAware();

	InitLogger();

	RenderParam renderParam;

	auto className = L"MyWindow";
	WNDCLASSW wndClass = {};
	wndClass.hInstance = hInstance;
	wndClass.lpszClassName = className;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
	{
		switch (msg)
		{
		case WM_SIZE:
		{
			auto renderParam = (RenderParam *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if (renderParam)
			{
				auto width = GET_X_LPARAM(lParam);
				auto height = GET_Y_LPARAM(lParam);

				if ((GetWindowLongPtr(hwnd, GWL_STYLE) == (WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS)))
				{
					RECT clientRect = {0, 0, 100, 100};
					AdjustWindowRect(&clientRect, WS_OVERLAPPEDWINDOW, FALSE);
					width = width - (clientRect.right - clientRect.left - 100);
					height = height - (clientRect.bottom - clientRect.top - 100);
				}

				renderParam->resizeSwapChain(width, height);
			}

			return 0;
		}
        case WM_NCHITTEST: {
            if (GetKeyState(VK_MENU) & 0x8000) {
                RECT windowRect;
                GetWindowRect(hwnd, &windowRect);
                long x = GET_X_LPARAM(lParam);
                long y = GET_Y_LPARAM(lParam);
                const int borderWidth = 8;

                if (x >= windowRect.left && x < windowRect.left + borderWidth && y >= windowRect.top && y < windowRect.top + borderWidth) return HTTOPLEFT;
                if (x < windowRect.right && x >= windowRect.right - borderWidth && y >= windowRect.top && y < windowRect.top + borderWidth) return HTTOPRIGHT;
                if (x >= windowRect.left && x < windowRect.left + borderWidth && y < windowRect.bottom && y >= windowRect.bottom - borderWidth) return HTBOTTOMLEFT;
                if (x < windowRect.right && x >= windowRect.right - borderWidth && y < windowRect.bottom && y >= windowRect.bottom - borderWidth) return HTBOTTOMRIGHT;
                if (x >= windowRect.left && x < windowRect.left + borderWidth) return HTLEFT;
                if (x < windowRect.right && x >= windowRect.right - borderWidth) return HTRIGHT;
                if (y >= windowRect.top && y < windowRect.top + borderWidth) return HTTOP;
                if (y < windowRect.bottom && y >= windowRect.bottom - borderWidth) return HTBOTTOM;
                
                return HTCAPTION;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
		case WM_DROPFILES:
		{
			HDROP hDrop = (HDROP)wParam;

			wchar_t filePath[MAX_PATH];
			// for (UINT i = 0; i < fileCount; i++)
			// {
			// 	DragQueryFile(hDrop, i, filePath, MAX_PATH);
			// 	MessageBox(hwnd, filePath, TEXT("Dropped File"), MB_OK);
			// }
			DragQueryFile(hDrop, 0, filePath, MAX_PATH);

			auto renderParam = (RenderParam *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			uint32_t ret = renderParam->InitVideoCtx(w2u(filePath).c_str());
			if (ret != 0)
			{
				logger->error("videoctx reinit failed");
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
					mi.rcMonitor.left,
					mi.rcMonitor.top,
					mi.rcMonitor.right - mi.rcMonitor.left,
					mi.rcMonitor.bottom - mi.rcMonitor.top,
					SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
				} else {
					SetWindowPos(hwnd, HWND_NOTOPMOST,
					g_windowedRect.left,
					g_windowedRect.top,
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
	};

	RegisterClass(&wndClass);
	auto window = CreateWindow(className, L"Hello World 测试程序", WS_POPUP | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, NULL, NULL, hInstance, NULL);

	DragAcceptFiles(window, TRUE);
	ShowWindow(window, SW_SHOW);
	SetForegroundWindow(window);

	RECT clientRect;
	GetClientRect(window, &clientRect);
	int clientWidth = clientRect.right - clientRect.left;
	int clientHeight = clientRect.bottom - clientRect.top;

	// D3D11
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	auto &bufferDesc = swapChainDesc.BufferDesc;
	bufferDesc.Width = clientWidth;
	bufferDesc.Height = clientHeight;
	bufferDesc.Format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
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

	IDXGISwapChain *swapChain;
	ID3D11Device *d3ddeivce;
	ID3D11DeviceContext *d3ddeviceCtx;
	D3D_FEATURE_LEVEL level;
	UINT flags = 0;
#ifdef DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // DEBUG
	flags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, NULL, NULL, D3D11_SDK_VERSION, &swapChainDesc, &swapChain, &d3ddeivce, &level, &d3ddeviceCtx);

	renderParam.Init(clientWidth, clientHeight, d3ddeivce, d3ddeviceCtx, swapChain);

	SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)&renderParam);

	MSG msg;

	while (1)
	{
		BOOL hasMsg = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
		if (hasMsg)
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			renderParam.render(window);
		}
	}

	CoUninitialize();
	return 0;
}
