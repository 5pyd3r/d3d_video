#ifndef PLAYBACK_TEXTUREUPDATER_H
#define PLAYBACK_TEXTUREUPDATER_H

#include <cstdint>
#include <d3d11.h>

struct AVFrame;
namespace nv { class VideoQuad; }

class TextureUpdater {
public:
    static void Update(HANDLE sharedHandle,
                       AVFrame* frame,
                       int& inOutWidth,
                       int& inOutHeight,
                       nv::VideoQuad* vq);
};

#endif
