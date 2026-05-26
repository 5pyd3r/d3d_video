#ifndef DETECT_NOOPACTION_H
#define DETECT_NOOPACTION_H

#include "IAction.h"

class NoopAction : public IAction {
public:
    void OnDetection(const DetectionResult&, const VideoFrame&) override {}
    const char* Name() const override { return "NoopAction"; }
};

#endif
