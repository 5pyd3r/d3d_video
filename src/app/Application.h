#ifndef APP_APPLICATION_H
#define APP_APPLICATION_H

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <memory>

#include "../playback/PlaybackController.h"

class Application {
public:
    int Run(HINSTANCE hInstance);
    ~Application();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool InitD3D11(HWND window);

    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_deviceCtx = nullptr;
    std::unique_ptr<PlaybackController> m_playback;
    HWND m_window = nullptr;
};

#endif
