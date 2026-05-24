#include "CaptureSource.h"
#include "../platform/Logger.h"

CaptureSource::CaptureSource(HWND targetWindow, ID3D11Device* device)
    : m_targetWindow(targetWindow), m_device(device) {}

CaptureSource::~CaptureSource() { Close(); }

bool CaptureSource::Init() {
    char buf[128];
    snprintf(buf, sizeof(buf), "Capture: 0x%llX", (uint64_t)m_targetWindow);
    m_title = buf;
    return m_capture.StartCapture(m_targetWindow, m_device);
}

bool CaptureSource::ReadFrame(VideoFrame& out) {
    // ProcessFrame writes directly to VideoQuad texture.
    // For the unified interface, we signal to the caller to use DrawCapture().
    // The frame type is Capture — caller handles accordingly.
    out.type = SourceType::Capture;
    out.width = m_capture.GetWidth();
    out.height = m_capture.GetHeight();
    return m_capture.IsCapturing();
}

void CaptureSource::Close() {
    m_capture.StopCapture();
}
