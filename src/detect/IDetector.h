#ifndef DETECT_IDETECTOR_H
#define DETECT_IDETECTOR_H

#include <d3d11.h>

struct VideoFrame;

struct DetectionResult {
    bool detected = false;
    const char* label = nullptr;
};

class IDetector {
public:
    virtual ~IDetector() = default;
    virtual DetectionResult Detect(const VideoFrame& frame,
                                    ID3D11DeviceContext* ctx) = 0;
    virtual const char* Name() const = 0;
};

#endif
