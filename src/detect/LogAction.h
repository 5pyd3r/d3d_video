#ifndef DETECT_LOGACTION_H
#define DETECT_LOGACTION_H

#include "IAction.h"
#include <cstdint>

class LogAction : public IAction {
public:
    LogAction() = default;

    void OnDetection(const DetectionResult& result,
                      const VideoFrame& frame) override;
    const char* Name() const override { return "LogAction"; }

private:
    uint32_t m_frameCount = 0;
    static constexpr uint32_t kLogInterval = 60;
};

#endif
