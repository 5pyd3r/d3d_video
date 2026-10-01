#include "SwapChainManager.h"
#include "../platform/Logger.h"

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

    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr) || !backBuffer) {
        logger->error("SwapChainManager::Init: GetBuffer failed: 0x{:08X}", (uint32_t)hr);
        return;
    }

    DXGI_SWAP_CHAIN_DESC sd = {};
    m_swapChain->GetDesc(&sd);

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = sd.BufferDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    hr = m_device->CreateRenderTargetView(backBuffer, &rtvDesc, &m_renderTargetView);
    backBuffer->Release();
    if (FAILED(hr)) {
        logger->error("SwapChainManager::Init: CreateRenderTargetView failed: 0x{:08X}", (uint32_t)hr);
        m_renderTargetView = nullptr;
    }
}

void SwapChainManager::Resize(int width, int height) {
    if (!m_swapChain || !m_device) return;
    // WM_SIZE already filters zero sizes; guard here too so a bad caller cannot
    // tear the render target down for a size that can never be created.
    if (width <= 0 || height <= 0) {
        logger->error("SwapChainManager::Resize: ignoring invalid size {}x{}", width, height);
        return;
    }

    if (m_renderTargetView) {
        m_renderTargetView->Release();
        m_renderTargetView = nullptr;
    }

    DXGI_SWAP_CHAIN_DESC desc = {};
    HRESULT hr = m_swapChain->GetDesc(&desc);
    if (FAILED(hr)) {
        logger->error("SwapChainManager::Resize: GetDesc failed: 0x{:08X}", (uint32_t)hr);
        return;
    }

    hr = m_swapChain->ResizeBuffers(desc.BufferCount, width, height, desc.BufferDesc.Format, desc.Flags);
    const bool resized = SUCCEEDED(hr);
    if (!resized) {
        logger->error("SwapChainManager::Resize: ResizeBuffers({}x{}) failed: 0x{:08X}, keeping the current buffers",
                      width, height, (uint32_t)hr);
    }

    // Rebuild the view either way: when ResizeBuffers failed the old buffers are
    // still current, and returning without a render target is what turned a failed
    // resize into a black window with no trace in the log.
    ID3D11Texture2D* backBuffer = nullptr;
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr) || !backBuffer) {
        logger->error("SwapChainManager::Resize: GetBuffer failed: 0x{:08X}", (uint32_t)hr);
        return;
    }

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = desc.BufferDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    hr = m_device->CreateRenderTargetView(backBuffer, &rtvDesc, &m_renderTargetView);
    backBuffer->Release();
    if (FAILED(hr)) {
        logger->error("SwapChainManager::Resize: CreateRenderTargetView failed: 0x{:08X}", (uint32_t)hr);
        m_renderTargetView = nullptr;
        return;
    }

    m_missingRtvLogged = false;

    // Only adopt the new size once the buffers really changed, otherwise the
    // viewport would describe a back buffer that does not exist.
    if (resized) {
        m_width = width;
        m_height = height;
    } else {
        logger->warn("SwapChainManager::Resize: viewport stays at {}x{}", m_width, m_height);
    }
}

void SwapChainManager::BeginFrame() {
    if (!m_deviceCtx || !m_renderTargetView) {
        if (!m_missingRtvLogged) {
            logger->error("SwapChainManager::BeginFrame: no render target view, frames are skipped");
            m_missingRtvLogged = true;
        }
        return;
    }

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
    if (!m_swapChain) return;

    HRESULT hr = m_swapChain->Present(1, 0);
    // A removed or reset device stops presenting for good; without this the window
    // simply freezes with nothing in the log to explain it. Note that
    // DXGI_STATUS_OCCLUDED (a parked or hidden window) is a success code and must
    // not be reported as a failure.
    if (FAILED(hr)) {
        if (!m_presentFailedLogged) {
            logger->error("SwapChainManager::EndFrame: Present failed: 0x{:08X}", (uint32_t)hr);
            m_presentFailedLogged = true;
        }
    } else {
        m_presentFailedLogged = false;
    }
}
