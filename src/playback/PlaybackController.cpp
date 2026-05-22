#include "PlaybackController.h"
#include "TextureUpdater.h"
#include "../platform/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

#include <spdlog/spdlog.h>

extern std::shared_ptr<spdlog::logger> logger;

PlaybackController::PlaybackController() : m_vq(nullptr), m_device(nullptr), m_deviceCtx(nullptr) {}

PlaybackController::~PlaybackController() {
    av_frame_free(&m_frame);
    m_decoder.Close();
    m_source.Close();
}

void PlaybackController::Init(ID3D11Device* device, ID3D11DeviceContext* deviceCtx,
                               IDXGISwapChain* swapChain, int viewWidth, int viewHeight) {
    m_device = device;
    m_deviceCtx = deviceCtx;
    m_viewWidth = viewWidth;
    m_viewHeight = viewHeight;

    m_swapChainMgr.Init(device, deviceCtx, swapChain, viewWidth, viewHeight);
    m_vq = std::make_unique<nv::VideoQuad>(device, deviceCtx, m_videoWidth, m_videoHeight);
}

uint32_t PlaybackController::LoadFile(const char* filePath) {
    uint32_t ret = m_source.Open(filePath);
    if (ret != 0) return ret;

    ret = m_decoder.Init(m_source.GetFormatContext(), m_frameDuration);
    if (ret != 0) return ret;

    m_frameCount = 0;
    m_state = PlayState::Play;
    m_startTime = std::chrono::steady_clock::now();
    return 0;
}

void PlaybackController::ResizeSwapChain(int width, int height) {
    m_viewWidth = width;
    m_viewHeight = height;
    m_swapChainMgr.Resize(width, height);
}

void PlaybackController::Draw(HWND hwnd) {
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

uint32_t PlaybackController::Render(HWND hwnd) {
    if (m_state == PlayState::Stop) return 0;

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - m_startTime;
    double presentTime = elapsed.count();
    double frameTime = m_frameDuration * m_frameCount;

    if (presentTime < frameTime || m_state == PlayState::Pause) {
        Draw(hwnd);
    } else {
        for (;;) {
            AVPacket* packet = m_source.ReadPacket();
            if (!packet) {
                av_frame_free(&m_frame);
                m_frame = nullptr;
                break;
            }

            auto decoded = m_decoder.SendAndReceive(packet);
            av_packet_free(&packet);

            if (decoded.type == AVMEDIA_TYPE_VIDEO) {
                av_frame_free(&m_frame);
                m_frame = decoded.frame;
                m_frameCount++;
                break;
            } else if (decoded.type == AVMEDIA_TYPE_AUDIO) {
                av_frame_free(&decoded.frame);
            }
        }

        if (m_frame == nullptr) {
            m_state = PlayState::Stop;
            return 0;
        }

        if (presentTime < frameTime + m_frameDuration) {
            TextureUpdater::Update(m_deviceCtx, m_vq->GetsharedHandle(),
                                    m_frame, m_videoWidth, m_videoHeight, m_vq.get());
        }
    }
    return 0;
}
