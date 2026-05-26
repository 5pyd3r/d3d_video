#include "LogAction.h"
#include "IDetector.h"
#include "../source/IVideoSource.h"
#include "../platform/Logger.h"
#include <cstdio>

void LogAction::OnDetection(const DetectionResult& result,
                             const VideoFrame& frame) {
    m_frameCount++;
    if (m_frameCount % kLogInterval != 1) return;

    char buf[256];
    snprintf(buf, sizeof(buf), "Detection: %s (frame=%dx%d, frame#=%u)",
             result.label ? result.label : "unknown",
             frame.width, frame.height, m_frameCount);
    logger->info(buf);
}
