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
#include <sstream>

#include <Windows.h>
#include <windowsx.h>
#include <ShlObj.h>
#include <wrl.h>
#include <DbgHelp.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>

#include "VideoCtx.h"
#include "VideoQuad.h"
#include "RenderParam.h"
#include <iomanip>

using namespace std::chrono;
namespace dx = DirectX;

using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::wstring;

std::shared_ptr<spdlog::logger> logger;

string w2s(const wstring& wstr) {
	int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), NULL, 0, NULL, NULL);
	string str(len, '\0');
	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), &str[0], str.size(), NULL, NULL);
	return str;
}

string w2u(const wstring& wstr) {
	int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wstr.size(), NULL, 0, NULL, NULL);
	string str(len, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wstr.size(), &str[0], str.size(), NULL, NULL);
	return str;
}

void LogStackTrace(PCONTEXT context)
{
	STACKFRAME64 stackFrame = {};
	DWORD machineType = IMAGE_FILE_MACHINE_AMD64;

#if defined(_M_IX86)
	machineType = IMAGE_FILE_MACHINE_I386;
	stackFrame.AddrPC.Offset = context->Eip;
	stackFrame.AddrFrame.Offset = context->Ebp;
	stackFrame.AddrStack.Offset = context->Esp;
#elif defined(_M_X64)
	machineType = IMAGE_FILE_MACHINE_AMD64;
	stackFrame.AddrPC.Offset = context->Rip;
	stackFrame.AddrFrame.Offset = context->Rbp;
	stackFrame.AddrStack.Offset = context->Rsp;
#endif

	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrStack.Mode = AddrModeFlat;

	logger->critical("=== Exception Call Stack ===");

	for (int i = 0; i < 100; ++i)
	{
		if (!StackWalk64(machineType,
						 GetCurrentProcess(),
						 GetCurrentThread(),
						 &stackFrame,
						 context,
						 nullptr,
						 SymFunctionTableAccess64,
						 SymGetModuleBase64,
						 nullptr))
		{
			break;
		}

		if (stackFrame.AddrPC.Offset == 0)
			break;

		char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(char)] = {};
		PSYMBOL_INFO symbol = (PSYMBOL_INFO)symbolBuffer;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = MAX_SYM_NAME;

		DWORD64 displacement = 0;
		DWORD64 address = stackFrame.AddrPC.Offset;
		BOOL ret = SymFromAddr(GetCurrentProcess(), address, &displacement, symbol);

		if (ret)
		{
			logger->critical("{:>3} 0x{:016X} {}", i, address, symbol->Name);
		}
		else
		{

			logger->critical("{:>3} 0x{:016X}", i, address);
		}
	}
}

std::string GetLastErrorMessage(DWORD errorCode = GetLastError())
{
	LPSTR messageBuffer = nullptr;

	// 使用 FormatMessage 获取错误描述
	size_t size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPSTR>(&messageBuffer),
		0,
		nullptr);

	if (size == 0)
	{
		// 如果 FormatMessage 失败，返回通用错误
		return "Unknown error (FormatMessage failed)";
	}

	// 复制消息到 std::string
	std::string message(messageBuffer, size);

	// 清理缓冲区
	LocalFree(messageBuffer);

	// 移除末尾的换行符
	if (!message.empty() && message.back() == '\n')
	{
		message.pop_back();
	}
	if (!message.empty() && message.back() == '\r')
	{
		message.pop_back();
	}

	return message;
}

void LogModuleInfo(PVOID address)
{
	IMAGEHLP_MODULE64 moduleInfo = {};
	moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
	if (SymGetModuleInfo64(GetCurrentProcess(), (DWORD64)address, &moduleInfo))
	{
		logger->critical("Module: {} (Base: 0x{:016X}, Size: {})",
						 moduleInfo.ModuleName, moduleInfo.BaseOfImage, moduleInfo.ImageSize);
	}
	else
	{
		DWORD errorCode = GetLastError();
		logger->critical("SysmGetModuleInfo64 failed: [{}]", errorCode);
	}
}

void LogRegisterState(PCONTEXT pContext)
{
	std::stringstream ss;
	ss << "=== Register State ===";

#if defined(_M_X64)
	// x64 寄存器
	ss << "\nRAX: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rax
	   << "  RBX: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rbx
	   << "  RCX: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rcx
	   << "  RDX: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rdx
	   << "\nRSI: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rsi
	   << "  RDI: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rdi
	   << "  RBP: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rbp
	   << "  RSP: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rsp
	   << "\nR8:  0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R8
	   << "  R9:  0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R9
	   << "  R10: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R10
	   << "  R11: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R11
	   << "\nR12: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R12
	   << "  R13: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R13
	   << "  R14: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R14
	   << "  R15: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R15
	   << "\nRIP: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rip
	   << "  EFLAGS: 0x" << std::hex << pContext->EFlags;
#elif defined(_M_IX86)
	// x86 寄存器
	ss << "\nEAX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Eax
	   << "  EBX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Ebx
	   << "  ECX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Ecx
	   << "  EDX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Edx
	   << "\nESI: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Esi
	   << "  EDI: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Edi
	   << "  EBP: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Ebp
	   << "  ESP: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Esp
	   << "\nEIP: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Eip
	   << "  EFLAGS: 0x" << std::hex << pContext->EFlags;
#endif

	// 段寄存器
	ss << "\nCS: 0x" << std::hex << pContext->SegCs
	   << "  DS: 0x" << pContext->SegDs
	   << "  ES: 0x" << pContext->SegEs
	   << "  FS: 0x" << pContext->SegFs
	   << "  GS: 0x" << pContext->SegGs
	   << "  SS: 0x" << pContext->SegSs;

	logger->critical(ss.str());
}

LONG WINAPI ExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
	LogRegisterState(pExceptionInfo->ContextRecord);
	LogModuleInfo(pExceptionInfo->ExceptionRecord->ExceptionAddress);
	LogStackTrace(pExceptionInfo->ContextRecord);
	return EXCEPTION_CONTINUE_SEARCH; // 继续执行默认异常处理
}

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
	SymInitialize(GetCurrentProcess(), nullptr, TRUE);
	SetUnhandledExceptionFilter(ExceptionHandler);

	CoInitializeEx(NULL, COINIT::COINIT_MULTITHREADED);
	SetProcessDPIAware();

	logger = spdlog::basic_logger_mt("file_logger", "d3d_video.log");
	logger->set_level(spdlog::level::debug);
	logger->flush_on(spdlog::level::warn);

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
