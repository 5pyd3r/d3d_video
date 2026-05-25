#ifndef SOURCE_FILESOURCE_H
#define SOURCE_FILESOURCE_H

#include "IVideoSource.h"
#include "MediaSource.h"
#include "../decode/VideoDecoder.h"
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
    const char* GetTitle() const override { return m_title.c_str(); }
    double GetFrameDuration() const override { return m_frameDuration; }

private:
    std::string m_path;
    std::string m_title;
    ID3D11Device* m_d3dDevice;
    MediaSource m_mediaSource;
    VideoDecoder m_decoder;
    AVFrame* m_frame = nullptr;
    double m_frameRate = 0.0;
    double m_frameDuration = 1.0 / 30.0;
    int m_width = 800;
    int m_height = 600;
};

#endif
