#include "VideoController.h"
#include "../platform/Logger.h"

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
        logger->info("SetSource: '{}' {}x{} @{:.2f}fps",
            source->GetTitle(), source->GetWidth(), source->GetHeight(),
            1.0 / source->GetFrameDuration());
        m_source = std::move(source);
        m_videoWidth = m_source->GetWidth();
        m_videoHeight = m_source->GetHeight();
        m_frameDuration = m_source->GetFrameDuration();
        m_frameCount = 0;
        m_startTime = std::chrono::steady_clock::now();
        m_lastSourceTitle = m_source->GetTitle();
        m_state = PlayState::Play;
        UpdatePowerOverride(true);
    } else if (source) {
        logger->error("SetSource: Init failed for '{}'", source->GetTitle());
    }
}

void VideoController::StopSource() {
    if (m_source) {
        m_source->Close();
        m_source.reset();
    }
    UpdatePowerOverride(false);
    m_state = PlayState::Stop;
}

void VideoController::Pause() {
    if (m_state != PlayState::Play) return;
    m_pausedTime = std::chrono::steady_clock::now();
    m_state = PlayState::Pause;
}

void VideoController::Resume() {
    if (m_state != PlayState::Pause) return;
    m_startTime += std::chrono::steady_clock::now() - m_pausedTime;
    m_state = PlayState::Play;
}

void VideoController::ResizeSwapChain(int width, int height) {
    m_viewWidth = width;
    m_viewHeight = height;
    m_swapChainMgr.Resize(width, height);
}

void VideoController::OnSystemSuspend() {
    Pause();
}

void VideoController::OnSystemResume() {
    Resume();
}

void VideoController::Draw(HWND hwnd) {
    m_swapChainMgr.BeginFrame();
    m_vq->BeginDraw();
    RECT rect;
    GetClientRect(hwnd, &rect);
    double srcRatio = (double)m_videoWidth / m_videoHeight;
    double dstRatio = (double)rect.right / rect.bottom;
    m_vq->UpdateByRatio(srcRatio, dstRatio);
    if (m_source) {
        m_vq->Draw(m_source->GetRenderDescriptor(m_vq.get()));
    } else {
        m_vq->Draw();  // default NV12 pipeline when no source
    }
    m_swapChainMgr.EndFrame();
    ClipCursor(NULL);
}

uint32_t VideoController::Render(HWND hwnd) {
    if (m_state == PlayState::Stop || !m_source) {
        Draw(hwnd);
        return 0;
    }

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - m_startTime;
    double presentTime = elapsed.count();
    double frameTime = m_frameDuration * m_frameCount;

    if (presentTime < frameTime || m_state == PlayState::Pause) {
        Draw(hwnd);
        return 0;
    }

    VideoFrame frame = {};
    FrameResult result = m_source->ReadFrame(frame, m_deviceCtx, m_vq.get());

    if (result == FrameResult::Got) {
        m_frameCount++;
        if (frame.width > 0 && frame.height > 0) {
            m_videoWidth = frame.width;
            m_videoHeight = frame.height;
        }
        UpdateWindowTitle(hwnd);
    } else if (result == FrameResult::End) {
        logger->info("VideoController: end of stream, stopping");
        m_state = PlayState::Stop;
        UpdatePowerOverride(false);
        UpdateWindowTitle(hwnd);
    }
    // NotReady: keep current state, redraw existing frame
    Draw(hwnd);
    return 0;
}

void VideoController::UpdatePowerOverride(bool playing) {
    if (playing && !m_powerOverrideActive) {
        SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
        m_powerOverrideActive = true;
        logger->info("VideoController: display/sleep override engaged");
    } else if (!playing && m_powerOverrideActive) {
        SetThreadExecutionState(ES_CONTINUOUS);
        m_powerOverrideActive = false;
        logger->info("VideoController: display/sleep override released");
    }
}

void VideoController::UpdateWindowTitle(HWND hwnd) {
    if (!m_source) return;
    const char* title = m_source->GetTitle();
    if (title && m_lastSourceTitle != title) {
        int len = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
        if (len > 0) {
            std::wstring wtitle(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, title, -1, &wtitle[0], len);
            SetWindowTextW(hwnd, wtitle.c_str());
        }
        m_lastSourceTitle = title;
    }
}
