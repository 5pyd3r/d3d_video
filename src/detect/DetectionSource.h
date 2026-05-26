#ifndef DETECT_DETECTIONSOURCE_H
#define DETECT_DETECTIONSOURCE_H

#include <memory>
#include <vector>
#include "../source/IVideoSource.h"
#include "IDetector.h"
#include "IAction.h"

class DetectionSource : public IVideoSource {
public:
    explicit DetectionSource(std::unique_ptr<IVideoSource> inner);

    void AddDetector(std::unique_ptr<IDetector> detector);
    void AddAction(std::unique_ptr<IAction> action);

    // IVideoSource
    bool Init() override;
    FrameResult ReadFrame(VideoFrame& out, ID3D11DeviceContext* ctx, nv::VideoQuad* vq) override;
    void Close() override;
    int GetWidth() const override;
    int GetHeight() const override;
    const char* GetTitle() const override;
    double GetFrameDuration() const override;
    RenderDescriptor GetRenderDescriptor(nv::VideoQuad* vq) const override;

private:
    std::unique_ptr<IVideoSource> m_inner;
    std::vector<std::unique_ptr<IDetector>> m_detectors;
    std::vector<std::unique_ptr<IAction>> m_actions;
};

#endif
