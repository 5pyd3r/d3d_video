#ifndef PLAYBACK_PLAYBACKCONTROLLER_H
#define PLAYBACK_PLAYBACKCONTROLLER_H

#include <cstdint>
#include <chrono>
#include <memory>
#include <vector>
#include <d3d11.h>
#include "../source/MediaSource.h"
#include "../decode/VideoDecoder.h"
#include "../render/VideoQuad.h"
#include "../render/SwapChainManager.h"

enum class PlayState { Play = 0, Pause = 1, Stop = 2 };

class PlaybackController {
public:
    PlaybackController();
    ~PlaybackController();

    void Init(ID3D11Device* device, ID3D11DeviceContext* deviceCtx,
              IDXGISwapChain* swapChain, int viewWidth, int viewHeight);

    uint32_t LoadFile(const char* filePath);
    void SetPlaylist(const std::vector<std::string>& files);
    uint32_t Render(HWND hwnd);
    void ResizeSwapChain(int width, int height);
    PlayState GetState() const { return m_state; }
    const std::string& GetCurrentFilePath() const { return m_currentFilePath; }
    void Draw(HWND hwnd);
    nv::VideoQuad* GetVideoQuad() const { return m_vq.get(); }
    SwapChainManager& GetSwapChainMgr() { return m_swapChainMgr; }

private:

    MediaSource m_source;
    VideoDecoder m_decoder;
    SwapChainManager m_swapChainMgr;
    std::unique_ptr<nv::VideoQuad> m_vq;

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceCtx;

    int m_videoWidth = 800;
    int m_videoHeight = 600;
    int m_viewWidth;
    int m_viewHeight;

    AVFrame* m_frame = nullptr;
    int m_frameCount = 0;
    double m_frameDuration;
    PlayState m_state = PlayState::Stop;
    std::vector<std::string> m_playlist;
    int m_playlistIndex = -1;
    std::chrono::steady_clock::time_point m_startTime;
    std::string m_currentFilePath;
    std::wstring m_lastWindowTitle;
};

#endif
