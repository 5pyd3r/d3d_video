#ifndef DETECT_VIDEOPROCSOURCE_H
#define DETECT_VIDEOPROCSOURCE_H

#include <memory>
#include <vector>
#include "../source/IVideoSource.h"
#include "IDetector.h"
#include "IAction.h"
#include "IFilter.h"

class VideoProcSource : public IVideoSource {
public:
    explicit VideoProcSource(std::unique_ptr<IVideoSource> inner);

    void AddDetector(std::unique_ptr<IDetector> detector);
    void AddAction(std::unique_ptr<IAction> action);
    void AddFilter(std::unique_ptr<IFilter> filter);

    void ToggleFilter();
    bool IsFilterEnabled() const { return m_activeFilter >= 0; }

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
    std::vector<std::unique_ptr<IFilter>> m_filters;
    int m_activeFilter = -1;
};

#endif
