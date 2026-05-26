#include "ScreenCapture.h"
#include "../render/VideoQuad.h"
#include "Logger.h"

#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>

// windows.graphics.directx.direct3d11.interop.h is NOT included because its
// namespace layout conflicts with Clang. We declare the needed symbols manually.

#include <dxgi.h>
#include <cstdint>

#pragma comment(lib, "windowsapp.lib")

// Forward-declare CreateDirect3D11DeviceFromDXGIDevice from the interop header
extern "C" HRESULT WINAPI CreateDirect3D11DeviceFromDXGIDevice(
    IDXGIDevice* dxgiDevice,
    IInspectable** outDirect3D11Device);

// IDirect3DDxgiInterfaceAccess: COM interop to extract DXGI interfaces from WinRT surfaces.
// Defined manually (guarded) to avoid namespace issues across Windows SDK versions.
#ifndef __IDirect3DDxgiInterfaceAccess_INTERFACE_DEFINED__
#define __IDirect3DDxgiInterfaceAccess_INTERFACE_DEFINED__
struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
IDirect3DDxgiInterfaceAccess : IUnknown {
    virtual HRESULT __stdcall GetInterface(REFIID riid, void** ppv) = 0;
};
#endif

namespace wrl = Microsoft::WRL;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::DirectX;

struct ScreenCapture::Impl {
    GraphicsCaptureItem item{ nullptr };
    Direct3D11CaptureFramePool framePool{ nullptr };
    GraphicsCaptureSession session{ nullptr };
};

ScreenCapture::ScreenCapture() : m_impl(std::make_unique<Impl>()) {}

ScreenCapture::~ScreenCapture() {
    StopCapture();
}

bool ScreenCapture::StartCapture(HWND targetWindow, ID3D11Device* device) {
    logger->info("ScreenCapture: starting capture for HWND=0x{:X}", (uint64_t)targetWindow);
    StopCapture();

    try {
        // 1. Create capture item via IGraphicsCaptureItemInterop
        auto interopFactory = winrt::get_activation_factory<GraphicsCaptureItem>();
        auto interop = interopFactory.as<IGraphicsCaptureItemInterop>();

        GraphicsCaptureItem item{ nullptr };
        winrt::check_hresult(
            interop->CreateForWindow(targetWindow,
                                     __uuidof(ABI::Windows::Graphics::Capture::IGraphicsCaptureItem),
                                     winrt::put_abi(item)));
        m_impl->item = item;

        // 2. Get capture size
        auto size = item.Size();
        m_width = size.Width;
        m_height = size.Height;
        if (m_width <= 0 || m_height <= 0) {
            logger->error("ScreenCapture: invalid capture size: {}x{}", m_width, m_height);
            m_impl->item = nullptr;
            return false;
        }

        // 3. Convert native ID3D11Device to WinRT IDirect3DDevice
        wrl::ComPtr<IDXGIDevice> dxgiDevice;
        winrt::check_hresult(
            device->QueryInterface(__uuidof(IDXGIDevice),
                                   reinterpret_cast<void**>(dxgiDevice.GetAddressOf())));

        winrt::com_ptr<::IInspectable> d3dInspectable;
        winrt::check_hresult(
            CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), d3dInspectable.put()));

        auto d3dDevice = d3dInspectable.as<IDirect3DDevice>();

        // 4. Create frame pool (BGRA)
        auto framePool = Direct3D11CaptureFramePool::Create(
            d3dDevice,
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            size);
        m_impl->framePool = framePool;

        // 5. Create session, disable cursor capture, start
        auto session = framePool.CreateCaptureSession(item);
        session.IsCursorCaptureEnabled(false);
        m_impl->session = session;

        session.StartCapture();

        m_isCapturing = true;
        logger->info("ScreenCapture: session started {}x{}", m_width, m_height);
        return true;

    } catch (winrt::hresult_error const& e) {
        logger->error("StartCapture failed: {} (0x{:08X})",
                      winrt::to_string(e.message()),
                      static_cast<uint32_t>(e.code()));
        m_impl->framePool = nullptr;
        m_impl->session = nullptr;
        m_impl->item = nullptr;
        return false;
    }
}

void ScreenCapture::StopCapture() {
    if (!m_isCapturing) return;
    m_isCapturing = false;

    m_impl->session = nullptr;
    m_impl->framePool = nullptr;
    m_impl->item = nullptr;
}

bool ScreenCapture::ProcessFrame(ID3D11DeviceContext* ctx, nv::VideoQuad* vq,
                                 int& outWidth, int& outHeight) {
    if (!m_impl->framePool) return false;

    auto frame = m_impl->framePool.TryGetNextFrame();
    if (!frame) return false;

    // Get D3D11 texture from WinRT surface
    auto surface = frame.Surface();
    auto* surfaceAbi = static_cast<::IUnknown*>(winrt::get_abi(surface));
    winrt::com_ptr<IDirect3DDxgiInterfaceAccess> dxgiAccess;
    winrt::check_hresult(
        surfaceAbi->QueryInterface(__uuidof(IDirect3DDxgiInterfaceAccess),
                                   dxgiAccess.put_void()));

    winrt::com_ptr<ID3D11Texture2D> texture;
    winrt::check_hresult(
        dxgiAccess->GetInterface(__uuidof(ID3D11Texture2D),
                                 texture.put_void()));

    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    if (outWidth != static_cast<int>(desc.Width) ||
        outHeight != static_cast<int>(desc.Height)) {
        outWidth = static_cast<int>(desc.Width);
        outHeight = static_cast<int>(desc.Height);
        vq->ResizeCapture(outWidth, outHeight);
    }

    // Copy BGRA capture frame into VideoQuad's capture texture
    ctx->CopySubresourceRegion(
        vq->GetCaptureTexture(), 0, 0, 0, 0,
        texture.get(), 0, nullptr);
    ctx->Flush();

    return true;
}
