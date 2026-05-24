#include "CaptureSource.h"
#include "../render/VideoQuad.h"
#include "../platform/Logger.h"

CaptureSource::CaptureSource(HWND targetWindow, ID3D11Device* device)
    : m_targetWindow(targetWindow), m_device(device) {}

CaptureSource::~CaptureSource() { Close(); }

bool CaptureSource::Init() {
    if (!m_capture.StartCapture(m_targetWindow, m_device)) return false;

    m_width = m_capture.GetWidth();
    m_height = m_capture.GetHeight();

    wchar_t wtitle[256] = {};
    GetWindowTextW(m_targetWindow, wtitle, 256);
    if (wcslen(wtitle) > 0) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wtitle, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            m_title.resize(len);
            WideCharToMultiByte(CP_UTF8, 0, wtitle, -1, &m_title[0], len, nullptr, nullptr);
            // Remove trailing null terminator
            if (!m_title.empty() && m_title.back() == '\0')
                m_title.pop_back();
        }
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "Window 0x%llX", (uint64_t)m_targetWindow);
        m_title = buf;
    }
    return true;
}

bool CaptureSource::ReadFrame(VideoFrame& out, ID3D11DeviceContext* ctx, nv::VideoQuad* vq) {
    if (!m_capture.IsCapturing()) return false;

    int w = m_width;
    int h = m_height;
    bool ok = m_capture.ProcessFrame(ctx, vq, w, h);
    m_width = w;
    m_height = h;

    out.texture = nullptr;
    out.type = SourceType::Capture;
    out.width = m_width;
    out.height = m_height;
    return ok;
}

void CaptureSource::Close() {
    m_capture.StopCapture();
}
