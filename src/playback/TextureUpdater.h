#ifndef PLAYBACK_TEXTUREUPDATER_H
#define PLAYBACK_TEXTUREUPDATER_H

#include <cstdint>
#include <d3d11.h>

struct AVFrame;
namespace nv { class VideoQuad; }

class TextureUpdater {
public:
    // True only for a frame the shared-texture copy path can consume: a D3D11
    // hardware frame carries the texture in data[0] and its subresource index in
    // data[1]. A decoder that fell back to software returns a CPU buffer in
    // data[0], and treating that as a texture faults inside D3D11, so every
    // caller gates on this predicate before touching the frame.
    static bool IsD3D11Frame(const AVFrame* frame);

    static void Update(ID3D11DeviceContext* deviceCtx,
                       HANDLE& sharedHandle,
                       AVFrame* frame,
                       int& inOutWidth,
                       int& inOutHeight,
                       nv::VideoQuad* vq);
};

#endif
