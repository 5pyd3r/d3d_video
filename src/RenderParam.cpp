#include "RenderParam.h"

RenderParam::RenderParam()
{
    this->vq = nullptr;
}

uint32_t RenderParam::Init(int width, int height, ID3D11Device *device, ID3D11DeviceContext *deviceCtx, IDXGISwapChain *swapChain)
{
    this->device = device;
    this->deviceCtx = deviceCtx;
    this->mySwap = swapChain;
    this->viewWidth = width;
    this->viewHeight = height;

    ID3D11Texture2D *myBack;
    mySwap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&myBack);

    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_B8G8R8A8_UNORM);
    device->CreateRenderTargetView(myBack, &renderTargetViewDesc, &this->m_d3dRenderTargetView);
    myBack->Release();

    this->vq = std::make_unique<nv::VideoQuad>(device, deviceCtx, this->videoWidth, this->videoHeight);

    return 0;
}

uint32_t RenderParam::InitVideoCtx(const char *filePath)
{
    uint32_t ret = this->ctx.Reinit(filePath, this->frame_duration);
    if (ret != 0)
    {
        return ret;
    }

    this->frameCount = 0;
    this->presentCount = 0;
    this->playStatus = 0;
    this->m_startTime = std::chrono::steady_clock::now();

    return 0;
}

RenderParam::~RenderParam()
{
}

uint32_t RenderParam::resize(AVFrame *frame)
{
    if (frame->width == this->videoWidth && frame->height == this->videoHeight)
    {
        return 0;
    }
    this->videoWidth = frame->width;
    this->videoHeight = frame->height;
    this->vq->Resize(this->videoHeight, this->videoWidth);
    return 0;
}

uint32_t RenderParam::resizeSwapChain(int width, int height)
{
    auto &mySwap = this->mySwap;
    auto &myDevice = this->device;

    this->viewWidth = width;
    this->viewHeight = height;

    this->m_d3dRenderTargetView->Release();
    auto ppRenderTarget = &this->m_d3dRenderTargetView;

    DXGI_SWAP_CHAIN_DESC SwapChainDesc;
    mySwap->GetDesc(&SwapChainDesc);
    mySwap->ResizeBuffers(SwapChainDesc.BufferCount, width, height, SwapChainDesc.BufferDesc.Format, SwapChainDesc.Flags);

    ID3D11Texture2D *myBack;
    mySwap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&myBack);

    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2D, SwapChainDesc.BufferDesc.Format);
    myDevice->CreateRenderTargetView(myBack, &renderTargetViewDesc, ppRenderTarget);
    myBack->Release();

    return 0;
}

uint32_t RenderParam::update(AVFrame *frame)
{
    this->resize(frame);

    ID3D11Texture2D *t_frame = (ID3D11Texture2D *)frame->data[0];
    int t_index = (int)frame->data[1];

    ID3D11Device *device;
    t_frame->GetDevice(&device);

    ID3D11DeviceContext *deviceCtx;
    device->GetImmediateContext(&deviceCtx);

    ID3D11Texture2D *videoTextureShared;
    device->OpenSharedResource(this->vq->GetsharedHandle(), __uuidof(ID3D11Texture2D), (void **)&videoTextureShared);

    deviceCtx->CopySubresourceRegion(videoTextureShared, 0, 0, 0, 0, t_frame, t_index, 0);
    deviceCtx->Flush();

    videoTextureShared->Release();
    deviceCtx->Release();
    device->Release();

    return 0;
}

uint32_t RenderParam::draw(HWND hwnd)
{
    auto &myDeviceCtx = this->deviceCtx;

    D3D11_VIEWPORT viewPort = {};
    viewPort.TopLeftX = 0;
    viewPort.TopLeftY = 0;
    viewPort.Width = this->viewWidth;
    viewPort.Height = this->viewHeight;
    viewPort.MaxDepth = 1;
    viewPort.MinDepth = 0;
    myDeviceCtx->RSSetViewports(1, &viewPort);

    myDeviceCtx->OMSetRenderTargets(1, &this->m_d3dRenderTargetView, nullptr);

    const FLOAT black[] = {0, 0, 0, 1};
    myDeviceCtx->ClearRenderTargetView(this->m_d3dRenderTargetView, black);

    this->vq->BeginDraw();

    RECT rect;
    GetClientRect(hwnd, &rect);
    double srcRatio = (double)this->videoWidth / this->videoHeight;
    double dstRatio = (double)rect.right / rect.bottom;
    this->vq->UpdateByRatio(srcRatio, dstRatio);

    this->vq->Draw();

    this->mySwap->Present(1, 0);

    ClipCursor(NULL);

    return 0;
}

uint32_t RenderParam::render(HWND hwnd)
{
    // 关键修改：使用高精度时钟获取真实的流逝时间
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - this->m_startTime;
    double presentTime = elapsed.count();

    double frameTime = this->frame_duration * this->frameCount;

    if (presentTime < frameTime || this->playStatus != 0)
    {
        this->currentSec = frameTime;
        this->draw(hwnd);
    }
    else
    {
        for (;;)
        {
            auto mediaFrame = this->ctx.nextFrame();

            if (mediaFrame.type == AVMediaType::AVMEDIA_TYPE_VIDEO)
            {
                av_frame_free(&frame);    // 假设 frame 是 AVFrame* 成员
                frame = mediaFrame.frame; // 存在内存管理风险
                this->frameCount++;
                break;
            }
            else if (mediaFrame.type == AVMediaType::AVMEDIA_TYPE_AUDIO)
            {
                av_frame_free(&mediaFrame.frame);
            }
            else if (mediaFrame.frame == nullptr)
            {
                frame = nullptr;
                break;
            }
        }

        if (frame == nullptr)
        {
            this->playStatus = 2;
            return 0;
        }

        if (presentTime < frameTime + this->frame_duration)
        {
            this->update(frame);
        }
    }

    return 0;
}