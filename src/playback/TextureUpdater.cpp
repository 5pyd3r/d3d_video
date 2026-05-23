#include "TextureUpdater.h"
#include "../render/VideoQuad.h"

extern "C" {
#include <libavutil/frame.h>
}

void TextureUpdater::Update(ID3D11DeviceContext* deviceCtx,
                             ID3D11Texture2D* dstTexture,
                             AVFrame* frame,
                             int& inOutWidth,
                             int& inOutHeight,
                             nv::VideoQuad* vq) {
    if (frame->width != inOutWidth || frame->height != inOutHeight) {
        inOutWidth = frame->width;
        inOutHeight = frame->height;
        vq->Resize(inOutHeight, inOutWidth);
        dstTexture = vq->GetVideoTexture();
    }

    ID3D11Texture2D* t_frame = (ID3D11Texture2D*)frame->data[0];
    int t_index = (int)(intptr_t)frame->data[1];

    deviceCtx->CopySubresourceRegion(dstTexture, 0, 0, 0, 0, t_frame, t_index, 0);
    deviceCtx->Flush();
}
