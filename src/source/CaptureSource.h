#ifndef SOURCE_CAPTURESOURCE_H
#define SOURCE_CAPTURESOURCE_H

#include "IVideoSource.h"
#include "../platform/ScreenCapture.h"
#include <string>

class CaptureSource : public IVideoSource {
public:
    CaptureSource(HWND targetWindow, ID3D11Device* device);
    ~CaptureSource() override;

    bool Init() override;
    FrameResult ReadFrame(VideoFrame& out, ID3D11DeviceContext* ctx, nv::VideoQuad* vq) override;
    void Close() override;
    int GetWidth() const override { return m_width; }
    int GetHeight() const override { return m_height; }
    const char* GetTitle() const override { return m_title.c_str(); }
    double GetFrameDuration() const override { return 1.0 / 30.0; }
    RenderDescriptor GetRenderDescriptor(nv::VideoQuad* vq) const override;

    HWND GetTargetWindow() const { return m_targetWindow; }

private:
    HWND m_targetWindow;
    ID3D11Device* m_device;
    ScreenCapture m_capture;
    std::string m_title;
    int m_width = 0;
    int m_height = 0;
};

#endif
