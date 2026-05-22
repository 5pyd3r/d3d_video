#include "RenderParam.h"
#include "platform/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

RenderParam::RenderParam()
{
    this->vq = nullptr;
}

uint32_t RenderParam::Init(int width, int height, ID3D11Device *device, ID3D11DeviceContext *deviceCtx, IDXGISwapChain *swapChain)
{
    this->device = device;
    this->deviceCtx = deviceCtx;
    this->viewWidth = width;
    this->viewHeight = height;

    swapChainMgr.Init(device, deviceCtx, swapChain, width, height);
    this->vq = std::make_unique<nv::VideoQuad>(device, deviceCtx, this->videoWidth, this->videoHeight);

    return 0;
}

uint32_t RenderParam::InitVideoCtx(const char *filePath) {
    uint32_t ret = source.Open(filePath);
    if (ret != 0) return ret;

    ret = decoder.Init(source.GetFormatContext(), this->frame_duration);
    if (ret != 0) return ret;

    this->frameCount = 0;
    this->presentCount = 0;
    this->playStatus = 0;
    this->m_startTime = std::chrono::steady_clock::now();
    return 0;
}

RenderParam::~RenderParam() {
    decoder.Close();
    source.Close();
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
    this->viewWidth = width;
    this->viewHeight = height;
    swapChainMgr.Resize(width, height);
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
    swapChainMgr.BeginFrame();

    this->vq->BeginDraw();
    RECT rect;
    GetClientRect(hwnd, &rect);
    double srcRatio = (double)this->videoWidth / this->videoHeight;
    double dstRatio = (double)rect.right / rect.bottom;
    this->vq->UpdateByRatio(srcRatio, dstRatio);
    this->vq->Draw();

    swapChainMgr.EndFrame();
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
        for (;;) {
            AVPacket* packet = source.ReadPacket();
            if (!packet) {
                frame = nullptr;
                break;
            }

            auto decoded = decoder.SendAndReceive(packet);
            av_packet_free(&packet);

            if (decoded.type == AVMEDIA_TYPE_VIDEO) {
                av_frame_free(&frame);
                frame = decoded.frame;
                this->frameCount++;
                break;
            } else if (decoded.type == AVMEDIA_TYPE_AUDIO) {
                av_frame_free(&decoded.frame);
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