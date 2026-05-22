#include "TextureUpdater.h"
#include "../render/VideoQuad.h"

extern "C" {
#include <libavutil/frame.h>
}

void TextureUpdater::Update(ID3D11DeviceContext* deviceCtx,
                             HANDLE sharedHandle,
                             AVFrame* frame,
                             int& inOutWidth,
                             int& inOutHeight,
                             nv::VideoQuad* vq) {
    if (frame->width != inOutWidth || frame->height != inOutHeight) {
        inOutWidth = frame->width;
        inOutHeight = frame->height;
        vq->Resize(inOutHeight, inOutWidth);
    }

    ID3D11Texture2D* t_frame = (ID3D11Texture2D*)frame->data[0];
    int t_index = (int)(intptr_t)frame->data[1];

    ID3D11Device* dev;
    t_frame->GetDevice(&dev);
    ID3D11DeviceContext* dctx;
    dev->GetImmediateContext(&dctx);

    ID3D11Texture2D* videoTextureShared;
    dev->OpenSharedResource(sharedHandle, __uuidof(ID3D11Texture2D), (void**)&videoTextureShared);
    dctx->CopySubresourceRegion(videoTextureShared, 0, 0, 0, 0, t_frame, t_index, 0);
    dctx->Flush();

    videoTextureShared->Release();
    dctx->Release();
    dev->Release();
}
