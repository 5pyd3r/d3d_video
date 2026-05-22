#ifndef RENDER_SWAPCHAINMANAGER_H
#define RENDER_SWAPCHAINMANAGER_H

#include <cstdint>
#include <d3d11.h>
#include <dxgi1_2.h>

class SwapChainManager {
public:
    SwapChainManager();
    ~SwapChainManager();

    void Init(ID3D11Device* device, ID3D11DeviceContext* deviceCtx,
              IDXGISwapChain* swapChain, int width, int height);
    void Resize(int width, int height);
    void BeginFrame();
    void EndFrame();
    ID3D11RenderTargetView* GetRenderTargetView() const { return m_renderTargetView; }

private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceCtx;
    IDXGISwapChain* m_swapChain;
    ID3D11RenderTargetView* m_renderTargetView;
    int m_width;
    int m_height;
};

#endif
