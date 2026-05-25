#ifndef SOURCE_IVIDEOSOURCE_H
#define SOURCE_IVIDEOSOURCE_H

#include <d3d11.h>

// ==========================================================================
// IVideoSource Implementation Checklist
//
// When creating a new IVideoSource subclass, verify:
//   [ ] GetFrameDuration() — explicit override required (pure virtual)
//   [ ] GetTitle() — returns display-ready human-readable title, not internal path
//   [ ] ReadFrame() — File: false means EOF. Capture: false means no frame ready,
//       caller should retry; do NOT permanently stop.
//   [ ] Init() — succeed or fail cleanly, no partial state
//   [ ] Close() — idempotent, safe to call multiple times
// ==========================================================================

enum class SourceType { File, Capture };

struct VideoFrame {
    ID3D11Texture2D* texture = nullptr;
    int width = 0;
    int height = 0;
    SourceType type = SourceType::File;
};

namespace nv { class VideoQuad; }

class IVideoSource {
public:
    virtual ~IVideoSource() = default;
    virtual bool Init() = 0;
    virtual bool ReadFrame(VideoFrame& out, ID3D11DeviceContext* ctx, nv::VideoQuad* vq) = 0;
    virtual void Close() = 0;
    virtual SourceType GetType() const = 0;
    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    virtual const char* GetTitle() const = 0;
    virtual double GetFrameDuration() const = 0;
};

#endif
