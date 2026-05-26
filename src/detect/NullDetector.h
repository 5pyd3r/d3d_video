#ifndef DETECT_NULLDETECTOR_H
#define DETECT_NULLDETECTOR_H

#include "IDetector.h"

class NullDetector : public IDetector {
public:
    DetectionResult Detect(const VideoFrame&, ID3D11DeviceContext*) override {
        return {false, nullptr};
    }
    const char* Name() const override { return "NullDetector"; }
};

#endif
