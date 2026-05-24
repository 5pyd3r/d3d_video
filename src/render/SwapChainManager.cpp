#include "SwapChainManager.h"

SwapChainManager::SwapChainManager()
    : m_device(nullptr), m_deviceCtx(nullptr), m_swapChain(nullptr),
      m_renderTargetView(nullptr), m_width(0), m_height(0) {}

SwapChainManager::~SwapChainManager() {
    if (m_renderTargetView) {
        m_renderTargetView->Release();
        m_renderTargetView = nullptr;
    }
}

void SwapChainManager::Init(ID3D11Device* device, ID3D11DeviceContext* deviceCtx,
                             IDXGISwapChain* swapChain, int width, int height) {
    m_device = device;
    m_deviceCtx = deviceCtx;
    m_swapChain = swapChain;
    m_width = width;
    m_height = height;

    ID3D11Texture2D* backBuffer;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    m_device->CreateRenderTargetView(backBuffer, &rtvDesc, &m_renderTargetView);
    backBuffer->Release();
}

void SwapChainManager::Resize(int width, int height) {
    m_width = width;
    m_height = height;

    if (m_renderTargetView) {
        m_renderTargetView->Release();
        m_renderTargetView = nullptr;
    }

    DXGI_SWAP_CHAIN_DESC desc;
    m_swapChain->GetDesc(&desc);
    m_swapChain->ResizeBuffers(desc.BufferCount, width, height, desc.BufferDesc.Format, desc.Flags);

    ID3D11Texture2D* backBuffer;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = desc.BufferDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    m_device->CreateRenderTargetView(backBuffer, &rtvDesc, &m_renderTargetView);
    backBuffer->Release();
}

void SwapChainManager::BeginFrame() {
    D3D11_VIEWPORT viewPort = {};
    viewPort.TopLeftX = 0;
    viewPort.TopLeftY = 0;
    viewPort.Width = (float)m_width;
    viewPort.Height = (float)m_height;
    viewPort.MaxDepth = 1;
    viewPort.MinDepth = 0;

    m_deviceCtx->RSSetViewports(1, &viewPort);
    m_deviceCtx->OMSetRenderTargets(1, &m_renderTargetView, nullptr);

    const FLOAT black[] = {0, 0, 0, 1};
    m_deviceCtx->ClearRenderTargetView(m_renderTargetView, black);
}

void SwapChainManager::EndFrame() {
    m_swapChain->Present(1, 0);
}
