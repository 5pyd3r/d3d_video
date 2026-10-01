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
        vq->Resize(inOutWidth, inOutHeight);
        sharedHandle = vq->GetsharedHandle();
    }

    ID3D11Texture2D* t_frame = (ID3D11Texture2D*)frame->data[0];
    int t_index = (int)(intptr_t)frame->data[1];

    if (!sharedHandle) {
        logger->error("TextureUpdater: VideoQuad has no shared texture, frame dropped");
        return;
    }

    // VideoQuad opens the shared handle once and caches it; doing it here meant a
    // GetDevice + OpenSharedResource round trip on every single frame.
    ID3D11Texture2D* videoTextureShared = vq->GetSharedTextureForCopy(sharedHandle);
    if (!videoTextureShared) {
        logger->error("TextureUpdater: no copy target for the shared texture, frame dropped");
        return;
    }

    deviceCtx->CopySubresourceRegion(videoTextureShared, 0, 0, 0, 0, t_frame, t_index, 0);
    // The decoder reuses its own texture for the next frame, so this copy has to
    // be complete before that happens.
    deviceCtx->Flush();
}
