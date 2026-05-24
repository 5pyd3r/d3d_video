#ifndef SOURCE_CAPTURESOURCE_H
#define SOURCE_CAPTURESOURCE_H

#include "IVideoSource.h"
#include "../platform/ScreenCapture.h"

class CaptureSource : public IVideoSource {
public:
    CaptureSource(HWND targetWindow, ID3D11Device* device);
    ~CaptureSource() override;

    bool Init() override;
    bool ReadFrame(VideoFrame& out) override;
    void Close() override;
    SourceType GetType() const override { return SourceType::Capture; }
    int GetWidth() const override { return m_capture.GetWidth(); }
    int GetHeight() const override { return m_capture.GetHeight(); }
    const char* GetTitle() const override { return "Capture"; }

private:
    HWND m_targetWindow;
    ID3D11Device* m_device;
    ScreenCapture m_capture;
    std::string m_title;
};

#endif
