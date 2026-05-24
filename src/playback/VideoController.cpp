#include "VideoController.h"

VideoController::VideoController() : m_device(nullptr), m_deviceCtx(nullptr) {}

VideoController::~VideoController() {
    StopSource();
}

void VideoController::Init(ID3D11Device* device, ID3D11DeviceContext* deviceCtx,
                            IDXGISwapChain* swapChain, int viewWidth, int viewHeight) {
    m_device = device;
    m_deviceCtx = deviceCtx;
    m_viewWidth = viewWidth;
    m_viewHeight = viewHeight;

    m_swapChainMgr.Init(device, deviceCtx, swapChain, viewWidth, viewHeight);
    m_vq = std::make_unique<nv::VideoQuad>(device, deviceCtx, m_videoWidth, m_videoHeight);
}

void VideoController::SetSource(std::unique_ptr<IVideoSource> source) {
    StopSource();
    if (source && source->Init()) {
        m_source = std::move(source);
        m_videoWidth = m_source->GetWidth();
        m_videoHeight = m_source->GetHeight();
        m_state = PlayState::Play;
    }
}

void VideoController::StopSource() {
    if (m_source) {
        m_source->Close();
        m_source.reset();
    }
    m_state = PlayState::Stop;
}

void VideoController::ResizeSwapChain(int width, int height) {
    m_viewWidth = width;
    m_viewHeight = height;
    m_swapChainMgr.Resize(width, height);
}

void VideoController::Draw(HWND hwnd) {
    m_swapChainMgr.BeginFrame();
    m_vq->BeginDraw();
    RECT rect;
    GetClientRect(hwnd, &rect);
    double srcRatio = (double)m_videoWidth / m_videoHeight;
    double dstRatio = (double)rect.right / rect.bottom;
    m_vq->UpdateByRatio(srcRatio, dstRatio);
    m_vq->Draw();
    m_swapChainMgr.EndFrame();
    ClipCursor(NULL);
}

void VideoController::DrawCapture(HWND hwnd) {
    m_swapChainMgr.BeginFrame();
    m_vq->BeginDraw();
    RECT rect;
    GetClientRect(hwnd, &rect);
    double srcRatio = (double)m_videoWidth / m_videoHeight;
    double dstRatio = (double)rect.right / rect.bottom;
    m_vq->UpdateByRatio(srcRatio, dstRatio);
    m_vq->DrawCapture();
    m_swapChainMgr.EndFrame();
    ClipCursor(NULL);
}

uint32_t VideoController::Render(HWND hwnd) {
    if (m_state == PlayState::Stop || !m_source) {
        Draw(hwnd);
        return 0;
    }

    VideoFrame frame = {};
    m_source->ReadFrame(frame, m_deviceCtx, m_vq.get());

    if (frame.width > 0 && frame.height > 0) {
        m_videoWidth = frame.width;
        m_videoHeight = frame.height;
    }

    if (m_source->GetType() == SourceType::File) {
        Draw(hwnd);
    } else {
        DrawCapture(hwnd);
    }
    return 0;
}
