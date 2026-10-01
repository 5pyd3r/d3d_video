#ifndef PLAYBACK_VIDEOCONTROLLER_H
#define PLAYBACK_VIDEOCONTROLLER_H

#include <cstdint>
#include <chrono>
#include <memory>
#include <string>
#include <d3d11.h>
#include <windows.h>
#include "../source/IVideoSource.h"
#include "../render/VideoQuad.h"
#include "../render/SwapChainManager.h"

enum class PlayState { Play = 0, Pause = 1, Stop = 2 };

// Why playback is currently held. A window drag and a system suspend can overlap,
// so each caller owns exactly one reason: playback resumes only after every reason
// that paused it has been released, and no reason can revive a controller that is
// not paused (stopped, or a source that never opened).
enum class PauseReason : unsigned {
    WindowDrag = 0,
    SystemSuspend = 1,
};

class VideoController {
public:
    VideoController();
    ~VideoController();

    void Init(ID3D11Device* device, ID3D11DeviceContext* deviceCtx,
              IDXGISwapChain* swapChain, int viewWidth, int viewHeight);

    void SetSource(std::unique_ptr<IVideoSource> source);
    void StopSource();
    void Pause(PauseReason reason);
    void Resume(PauseReason reason);
    uint32_t Render(HWND hwnd);
    void OnSystemSuspend();
    void OnSystemResume();
    void SetHidden(bool hidden);
    void ResizeSwapChain(int width, int height);
    PlayState GetState() const { return m_state; }

    IVideoSource* GetSource() const { return m_source.get(); }
    nv::VideoQuad* GetVideoQuad() const { return m_vq.get(); }

private:
    void Draw(HWND hwnd);

    std::unique_ptr<IVideoSource> m_source;
    SwapChainManager m_swapChainMgr;
    std::unique_ptr<nv::VideoQuad> m_vq;

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceCtx;

    int m_videoWidth = 800;
    int m_videoHeight = 600;
    int m_viewWidth;
    int m_viewHeight;

    int m_frameCount = 0;
    double m_frameDuration = 1.0 / 30.0;
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_pausedTime;
    bool m_powerOverrideActive = false;
    bool m_hidden = false;
    std::string m_lastSourceTitle;

    void UpdatePowerOverride(bool playing);
    void UpdateWindowTitle(HWND hwnd);

    static unsigned ToPauseMask(PauseReason reason) {
        return 1u << static_cast<unsigned>(reason);
    }

    // One bit per reason currently holding playback.
    unsigned m_pauseReasons = 0;
    PlayState m_state = PlayState::Stop;
};

#endif
