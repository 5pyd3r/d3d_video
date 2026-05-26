#ifndef DETECT_IACTION_H
#define DETECT_IACTION_H

struct DetectionResult;
struct VideoFrame;

class IAction {
public:
    virtual ~IAction() = default;
    virtual void OnDetection(const DetectionResult& result,
                              const VideoFrame& frame) = 0;
    virtual const char* Name() const = 0;
};

#endif
