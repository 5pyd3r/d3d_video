#ifndef PLAYBACK_VIDEOCONTROLLER_H
#define PLAYBACK_VIDEOCONTROLLER_H

#include <cstdint>
#include <memory>
#include <d3d11.h>
#include "../source/IVideoSource.h"
#include "../render/VideoQuad.h"
#include "../render/SwapChainManager.h"

enum class PlayState { Play = 0, Pause = 1, Stop = 2 };

class VideoController {
public:
    VideoController();
    ~VideoController();

    void Init(ID3D11Device* device, ID3D11DeviceContext* deviceCtx,
              IDXGISwapChain* swapChain, int viewWidth, int viewHeight);

    void SetSource(std::unique_ptr<IVideoSource> source);
    void StopSource();
    uint32_t Render(HWND hwnd);
    void ResizeSwapChain(int width, int height);
    PlayState GetState() const { return m_state; }

    IVideoSource* GetSource() const { return m_source.get(); }
    nv::VideoQuad* GetVideoQuad() const { return m_vq.get(); }
    SwapChainManager& GetSwapChainMgr() { return m_swapChainMgr; }

private:
    void Draw(HWND hwnd);
    void DrawCapture(HWND hwnd);

    std::unique_ptr<IVideoSource> m_source;
    SwapChainManager m_swapChainMgr;
    std::unique_ptr<nv::VideoQuad> m_vq;

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceCtx;

    int m_videoWidth = 800;
    int m_videoHeight = 600;
    int m_viewWidth;
    int m_viewHeight;

    PlayState m_state = PlayState::Stop;
};

#endif
