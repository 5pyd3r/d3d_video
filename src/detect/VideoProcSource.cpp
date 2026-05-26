#include "VideoProcSource.h"
#include "../render/VideoQuad.h"

VideoProcSource::VideoProcSource(std::unique_ptr<IVideoSource> inner)
    : m_inner(std::move(inner)) {}

void VideoProcSource::AddDetector(std::unique_ptr<IDetector> detector) {
    m_detectors.push_back(std::move(detector));
}

void VideoProcSource::AddAction(std::unique_ptr<IAction> action) {
    m_actions.push_back(std::move(action));
}

void VideoProcSource::AddFilter(std::unique_ptr<IFilter> filter) {
    m_filters.push_back(std::move(filter));
}

void VideoProcSource::ToggleFilter() {
    if (m_filters.empty()) return;
    m_activeFilter++;
    if (m_activeFilter >= static_cast<int>(m_filters.size()))
        m_activeFilter = -1;
}

bool VideoProcSource::Init() {
    return m_inner ? m_inner->Init() : false;
}

FrameResult VideoProcSource::ReadFrame(VideoFrame& out, ID3D11DeviceContext* ctx, nv::VideoQuad* vq) {
    if (!m_inner) {
        out = {};
        return FrameResult::End;
    }
    FrameResult result = m_inner->ReadFrame(out, ctx, vq);
    if (result == FrameResult::Got) {
        for (auto& detector : m_detectors) {
            DetectionResult dr = detector->Detect(out, ctx);
            if (dr.detected) {
                for (auto& action : m_actions) {
                    action->OnDetection(dr, out);
                }
            }
        }
    }
    return result;
}

void VideoProcSource::Close() {
    if (m_inner) m_inner->Close();
}

int VideoProcSource::GetWidth() const {
    return m_inner ? m_inner->GetWidth() : 0;
}

int VideoProcSource::GetHeight() const {
    return m_inner ? m_inner->GetHeight() : 0;
}

const char* VideoProcSource::GetTitle() const {
    return m_inner ? m_inner->GetTitle() : "VideoProcSource";
}

double VideoProcSource::GetFrameDuration() const {
    return m_inner ? m_inner->GetFrameDuration() : 1.0 / 30.0;
}

RenderDescriptor VideoProcSource::GetRenderDescriptor(nv::VideoQuad* vq) const {
    if (!m_inner) return {};
    if (m_activeFilter < 0 || m_activeFilter >= static_cast<int>(m_filters.size()))
        return m_inner->GetRenderDescriptor(vq);
    if (!vq) return m_inner->GetRenderDescriptor(vq);
    auto* filter = m_filters[m_activeFilter].get();
    auto innerDesc = m_inner->GetRenderDescriptor(vq);
    RenderDescriptor desc = {};
    desc.srvs[0] = innerDesc.srvs[0];
    desc.srvs[1] = innerDesc.srvs[1];
    // Select shader based on inner source type: capture uses BGRA shader, NV12 uses NV12 shader
    if (innerDesc.pixelShader == vq->GetCapturePixelShader()) {
        desc.pixelShader = filter->GetBGRAShader();
    } else {
        desc.pixelShader = filter->GetNV12Shader();
    }
    return desc;
}
