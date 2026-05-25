#include "PlaylistSource.h"
#include "FileSource.h"
#include "../platform/Logger.h"

PlaylistSource::PlaylistSource(const std::vector<std::string>& files, ID3D11Device* device)
    : m_files(files), m_device(device) {}

PlaylistSource::~PlaylistSource() { Close(); }

bool PlaylistSource::Init() {
    if (m_files.empty()) return false;
    m_index = 0;
    return OpenCurrent();
}

bool PlaylistSource::OpenCurrent() {
    m_currentSource = std::make_unique<FileSource>(m_files[m_index].c_str(), m_device);
    if (m_currentSource->Init()) {
        m_currentTitle = m_currentSource->GetTitle();
        return true;
    }
    m_currentSource.reset();
    return false;
}

FrameResult PlaylistSource::ReadFrame(VideoFrame& out, ID3D11DeviceContext* ctx, nv::VideoQuad* vq) {
    if (!m_currentSource) return FrameResult::End;

    FrameResult result = m_currentSource->ReadFrame(out, ctx, vq);
    if (result == FrameResult::Got) return FrameResult::Got;
    if (result == FrameResult::NotReady) return FrameResult::NotReady;

    // Current file ended — try next
    if (m_index + 1 < m_files.size()) {
        m_currentSource.reset();
        m_index++;
        if (OpenCurrent())
            return m_currentSource->ReadFrame(out, ctx, vq);
    }
    return FrameResult::End;  // Playlist exhausted
}

void PlaylistSource::Close() {
    m_currentSource.reset();
}

int PlaylistSource::GetWidth() const {
    return m_currentSource ? m_currentSource->GetWidth() : 0;
}

int PlaylistSource::GetHeight() const {
    return m_currentSource ? m_currentSource->GetHeight() : 0;
}

const char* PlaylistSource::GetTitle() const {
    return m_currentSource ? m_currentSource->GetTitle() : "Playlist";
}

double PlaylistSource::GetFrameDuration() const {
    return m_currentSource ? m_currentSource->GetFrameDuration() : 1.0 / 30.0;
}

RenderDescriptor PlaylistSource::GetRenderDescriptor(nv::VideoQuad* vq) const {
    return m_currentSource ? m_currentSource->GetRenderDescriptor(vq)
                          : RenderDescriptor{};
}
