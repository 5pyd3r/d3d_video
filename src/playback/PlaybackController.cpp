#include "PlaybackController.h"
#include "TextureUpdater.h"
#include "../platform/Logger.h"
#include "../platform/StringUtils.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

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
    // Reset previous playback state before loading new file
    m_state = PlayState::Stop;
    av_frame_free(&m_frame);
    m_decoder.Close();
    m_source.Close();

    double frameRate = 0.0;
    uint32_t ret = m_source.Open(filePath);
    if (ret != 0) return ret;

    ret = m_decoder.Init(m_source.GetFormatContext(), frameRate, m_device);
    if (ret != 0) return ret;

    m_frameDuration = (frameRate > 0.0) ? (1.0 / frameRate) : (1.0 / 30.0);
    m_frameCount = 0;
    m_state = PlayState::Play;
    m_startTime = std::chrono::steady_clock::now();
    m_currentFilePath = filePath;
    return 0;
}

void PlaybackController::SetPlaylist(const std::vector<std::string>& files) {
    m_playlist = files;
    m_playlistIndex = 0;
    av_frame_free(&m_frame);
    m_decoder.Close();
    m_source.Close();
    if (!files.empty()) {
        LoadFile(files[0].c_str());
    }
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
    if (m_state == PlayState::Stop) {
        Draw(hwnd);
        return 0;
    }

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

        // Flush decoder if no frame yet
        if (m_frame == nullptr) {
            auto flushResult = m_decoder.Flush(0);  // video stream
            if (flushResult.type == AVMEDIA_TYPE_VIDEO && flushResult.frame) {
                m_frame = flushResult.frame;
                m_frameCount++;
            }
        }

        if (m_frame == nullptr) {
            if (m_playlistIndex >= 0 && m_playlistIndex + 1 < (int)m_playlist.size()) {
                m_playlistIndex++;
                LoadFile(m_playlist[m_playlistIndex].c_str());
                std::wstring title = TruncateFileNameForTitle(m_currentFilePath);
                if (title != m_lastWindowTitle) {
                    SetWindowTextW(hwnd, title.c_str());
                    m_lastWindowTitle = title;
                }
                return 0;
            }
            m_state = PlayState::Stop;
            m_currentFilePath.clear();
            return 0;
        }

        if (presentTime < frameTime + m_frameDuration) {
            HANDLE sharedHandle = m_vq->GetsharedHandle();
            TextureUpdater::Update(m_deviceCtx, sharedHandle,
                                    m_frame, m_videoWidth, m_videoHeight, m_vq.get());
        }
    }
    return 0;
}
