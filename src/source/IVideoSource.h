#ifndef SOURCE_IVIDEOSOURCE_H
#define SOURCE_IVIDEOSOURCE_H

#include <d3d11.h>

// ==========================================================================
// IVideoSource Implementation Checklist
//
// When creating a new IVideoSource subclass, verify:
//   [ ] GetFrameDuration() — explicit override required (pure virtual)
//   [ ] GetTitle() — returns display-ready human-readable title, not internal path
//   [ ] ReadFrame() — returns Got (frame decoded), NotReady (transient, retry),
//       or End (EOF/source stopped, transition to Stop state)
//   [ ] Init() — succeed or fail cleanly, no partial state
//   [ ] Close() — idempotent, safe to call multiple times
// ==========================================================================

enum class SourceType { File, Capture };

enum class FrameResult { Got, NotReady, End };

struct VideoFrame {
    ID3D11Texture2D* texture = nullptr;
    int width = 0;
    int height = 0;
    SourceType type = SourceType::File;
};

namespace nv { class VideoQuad; }

// Describes the GPU pipeline needed to render this source's frames.
// Each source provides its pixel shader + up to 2 texture SRVs.
struct RenderDescriptor {
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11ShaderResourceView* srvs[2] = {nullptr, nullptr};
};

class IVideoSource {
public:
    virtual ~IVideoSource() = default;
    virtual bool Init() = 0;
    virtual FrameResult ReadFrame(VideoFrame& out, ID3D11DeviceContext* ctx, nv::VideoQuad* vq) = 0;
    virtual void Close() = 0;
    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    virtual const char* GetTitle() const = 0;
    virtual double GetFrameDuration() const = 0;

    // Return the render pipeline for this source's current frame.
    // Called by VideoController to select the correct shader + textures.
    virtual RenderDescriptor GetRenderDescriptor(nv::VideoQuad* vq) const = 0;
};

#endif
