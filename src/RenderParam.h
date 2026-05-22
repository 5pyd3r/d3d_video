#ifndef RENDERPARAM_H
#define RENDERPARAM_H

#include <cstdint>
#include <chrono>
#include <memory>
#include <d3d11.h>
#include "source/MediaSource.h"
#include "decode/VideoDecoder.h"
#include "VideoQuad.h"

#define DEFAULT_VIDEO_WIDTH 800
#define DEFAULT_VIDEO_HEIGHT 600

class RenderParam {
public:
    RenderParam();
    ~RenderParam();
    uint32_t Init(int width, int height, ID3D11Device *device, ID3D11DeviceContext *deviceCtx, IDXGISwapChain *swapChain);
    uint32_t InitVideoCtx(const char *filePath);
    uint32_t resize(AVFrame *frame);
    uint32_t resizeSwapChain(int width, int height);
    uint32_t update(AVFrame *frame);
    uint32_t draw(HWND hwnd);
    uint32_t render(HWND hwnd);

private:
    int videoWidth = DEFAULT_VIDEO_WIDTH;
    int videoHeight = DEFAULT_VIDEO_HEIGHT;
    int viewWidth;
    int viewHeight;
    AVFrame *frame = nullptr;
    std::unique_ptr<nv::VideoQuad> vq = nullptr;
    ID3D11Device *device;
    ID3D11DeviceContext *deviceCtx;
    IDXGISwapChain *mySwap;
    ID3D11RenderTargetView *m_d3dRenderTargetView;
    int frameCount = 0;
    int presentCount = 0;
    std::chrono::steady_clock::time_point m_startTime;
    int playStatus = 2; // 0 play, 1 pause, 2 stop
    float currentSec;
    MediaSource source;
    VideoDecoder decoder;
    double frame_duration;
};

#endif // !RENDERPARAM_H