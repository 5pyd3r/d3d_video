#include "TextureUpdater.h"
#include "../render/VideoQuad.h"
#include "../platform/Logger.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

bool TextureUpdater::IsD3D11Frame(const AVFrame* frame) {
    return frame != nullptr &&
           frame->format == AV_PIX_FMT_D3D11 &&
           frame->data[0] != nullptr;
}

void TextureUpdater::Update(ID3D11DeviceContext* deviceCtx,
                             HANDLE& sharedHandle,
                             AVFrame* frame,
                             int& inOutWidth,
                             int& inOutHeight,
                             nv::VideoQuad* vq) {
    // Fail closed: a software frame holds a CPU pointer in data[0] and a plain
    // integer in data[1], so copying it as a texture + subresource index faults.
    if (!IsD3D11Frame(frame)) {
        logger->error("TextureUpdater: refusing a non-D3D11 frame (format={})",
                      frame ? static_cast<int>(frame->format) : -1);
        return;
    }

    if (frame->width != inOutWidth || frame->height != inOutHeight) {
        inOutWidth = frame->width;
        inOutHeight = frame->height;
        vq->Resize(inOutHeight, inOutWidth);
        sharedHandle = vq->GetsharedHandle();
    }

    ID3D11Texture2D* t_frame = (ID3D11Texture2D*)frame->data[0];
    int t_index = (int)(intptr_t)frame->data[1];

    if (!sharedHandle) {
        logger->error("TextureUpdater: VideoQuad has no shared texture, frame dropped");
        return;
    }

    ID3D11Device* dev = nullptr;
    deviceCtx->GetDevice(&dev);

    ID3D11Texture2D* videoTextureShared = nullptr;
    HRESULT hr = dev->OpenSharedResource(sharedHandle, __uuidof(ID3D11Texture2D), (void**)&videoTextureShared);
    if (FAILED(hr)) {
        logger->error("TextureUpdater: OpenSharedResource failed: 0x{:08X}", (uint32_t)hr);
        dev->Release();
        return;
    }
    deviceCtx->CopySubresourceRegion(videoTextureShared, 0, 0, 0, 0, t_frame, t_index, 0);
    deviceCtx->Flush();

    videoTextureShared->Release();
    dev->Release();
}
