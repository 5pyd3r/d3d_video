#ifndef SOURCE_FILESOURCE_H
#define SOURCE_FILESOURCE_H

#include "IVideoSource.h"
#include "MediaSource.h"
#include "../decode/VideoDecoder.h"
#include <chrono>
#include <string>

class FileSource : public IVideoSource {
public:
    FileSource(const char* path, ID3D11Device* d3dDevice);
    ~FileSource() override;

    bool Init() override;
    bool ReadFrame(VideoFrame& out, ID3D11DeviceContext* ctx, nv::VideoQuad* vq) override;
    void Close() override;
    SourceType GetType() const override { return SourceType::File; }
    int GetWidth() const override { return m_width; }
    int GetHeight() const override { return m_height; }
    const char* GetTitle() const override { return m_path.c_str(); }

private:
    std::string m_path;
    ID3D11Device* m_d3dDevice;
    MediaSource m_mediaSource;
    VideoDecoder m_decoder;
    AVFrame* m_frame = nullptr;
    double m_frameRate = 0.0;
    int m_width = 800;
    int m_height = 600;
    int m_frameCount = 0;
    double m_frameDuration = 0.0;
    std::chrono::steady_clock::time_point m_startTime;
};

#endif
