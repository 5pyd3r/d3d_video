#ifndef PLATFORM_SCREENCAPTURE_H
#define PLATFORM_SCREENCAPTURE_H

#include <windows.h>
#include <d3d11.h>
#include <memory>

namespace nv { class VideoQuad; }

class ScreenCapture {
public:
    ScreenCapture();
    ~ScreenCapture();

    bool StartCapture(HWND targetWindow, ID3D11Device* device);
    void StopCapture();
    bool IsCapturing() const { return m_isCapturing; }

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    bool ProcessFrame(ID3D11DeviceContext* ctx, nv::VideoQuad* vq,
                      int& outWidth, int& outHeight);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    bool m_isCapturing = false;
    int m_width = 0, m_height = 0;
};

#endif
