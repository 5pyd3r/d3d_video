#include <gtest/gtest.h>

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <memory>

#include "../../src/render/SwapChainManager.h"

namespace {

// Real device + swap chain with a hardware-then-WARP fallback, so the resize
// guards still execute on a runner without a discrete GPU.
class SwapChainManagerTest : public ::testing::Test {
protected:
    HWND m_hwnd = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_ctx = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    std::unique_ptr<SwapChainManager> m_mgr;

    void SetUp() override {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"SwapChainManagerTest";
        RegisterClassW(&wc);
        m_hwnd = CreateWindowExW(0, L"SwapChainManagerTest", L"", WS_POPUP,
                                 0, 0, 320, 240, nullptr, nullptr, wc.hInstance, nullptr);
        if (!m_hwnd) {
            GTEST_SKIP() << "cannot create the test window";
            return;
        }

        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferDesc.Width = 320;
        scd.BufferDesc.Height = 240;
        scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        scd.SampleDesc.Count = 1;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.BufferCount = 2;
        scd.OutputWindow = m_hwnd;
        scd.Windowed = TRUE;
        scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

        static const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1
        };
        const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, ARRAYSIZE(levels),
            D3D11_SDK_VERSION, &scd, &m_swapChain, &m_device, &got, &m_ctx);
        if (FAILED(hr)) {
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels, ARRAYSIZE(levels),
                D3D11_SDK_VERSION, &scd, &m_swapChain, &m_device, &got, &m_ctx);
        }
        if (FAILED(hr) || m_device == nullptr || m_swapChain == nullptr) {
            m_device = nullptr;
            m_ctx = nullptr;
            m_swapChain = nullptr;
            return;
        }

        m_mgr = std::make_unique<SwapChainManager>();
        m_mgr->Init(m_device, m_ctx, m_swapChain, 320, 240);
    }

    void TearDown() override {
        m_mgr.reset();
        if (m_ctx) { m_ctx->Release(); m_ctx = nullptr; }
        if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
        if (m_device) { m_device->Release(); m_device = nullptr; }
        if (m_hwnd) { DestroyWindow(m_hwnd); m_hwnd = nullptr; }
    }
};

}  // namespace

TEST_F(SwapChainManagerTest, InitCreatesRenderTargetView) {
    if (!m_mgr) GTEST_SKIP() << "no D3D11 device (hardware or WARP) available";
    EXPECT_NE(m_mgr->GetRenderTargetView(), nullptr);
}

TEST_F(SwapChainManagerTest, ResizeWithInvalidSizeKeepsTheRenderTargetView) {
    if (!m_mgr) GTEST_SKIP() << "no D3D11 device (hardware or WARP) available";
    ASSERT_NE(m_mgr->GetRenderTargetView(), nullptr);
    ID3D11RenderTargetView* before = m_mgr->GetRenderTargetView();

    m_mgr->Resize(0, 0);

    // The old code released the view first and then gave up, which left the frame
    // loop binding a null render target and presenting a black window.
    EXPECT_EQ(m_mgr->GetRenderTargetView(), before);

    m_mgr->BeginFrame();
    m_mgr->EndFrame();
}

TEST_F(SwapChainManagerTest, ResizeToValidSizeRebuildsTheViewAndRendersAFrame) {
    if (!m_mgr) GTEST_SKIP() << "no D3D11 device (hardware or WARP) available";
    ASSERT_NE(m_mgr->GetRenderTargetView(), nullptr);

    m_mgr->Resize(160, 120);
    ASSERT_NE(m_mgr->GetRenderTargetView(), nullptr);

    m_mgr->BeginFrame();
    m_mgr->EndFrame();

    m_mgr->Resize(320, 240);
    EXPECT_NE(m_mgr->GetRenderTargetView(), nullptr);
}
