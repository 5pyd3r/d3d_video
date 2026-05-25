#ifndef SOURCE_PLAYLISTSOURCE_H
#define SOURCE_PLAYLISTSOURCE_H

#include "IVideoSource.h"
#include <vector>
#include <string>
#include <memory>

class FileSource;

class PlaylistSource : public IVideoSource {
public:
    PlaylistSource(const std::vector<std::string>& files, ID3D11Device* device);
    ~PlaylistSource() override;

    bool Init() override;
    FrameResult ReadFrame(VideoFrame& out, ID3D11DeviceContext* ctx, nv::VideoQuad* vq) override;
    void Close() override;
    int GetWidth() const override;
    int GetHeight() const override;
    const char* GetTitle() const override;
    double GetFrameDuration() const override;
    RenderDescriptor GetRenderDescriptor(nv::VideoQuad* vq) const override;

private:
    bool OpenCurrent();

    std::vector<std::string> m_files;
    ID3D11Device* m_device;
    std::unique_ptr<FileSource> m_currentSource;
    size_t m_index = 0;
    std::string m_currentTitle;
};

#endif
