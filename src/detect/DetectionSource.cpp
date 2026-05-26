#include "DetectionSource.h"

DetectionSource::DetectionSource(std::unique_ptr<IVideoSource> inner)
    : m_inner(std::move(inner)) {}

void DetectionSource::AddDetector(std::unique_ptr<IDetector> detector) {
    m_detectors.push_back(std::move(detector));
}

void DetectionSource::AddAction(std::unique_ptr<IAction> action) {
    m_actions.push_back(std::move(action));
}

bool DetectionSource::Init() {
    return m_inner ? m_inner->Init() : false;
}

FrameResult DetectionSource::ReadFrame(VideoFrame& out, ID3D11DeviceContext* ctx, nv::VideoQuad* vq) {
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

void DetectionSource::Close() {
    if (m_inner) m_inner->Close();
}

int DetectionSource::GetWidth() const {
    return m_inner ? m_inner->GetWidth() : 0;
}

int DetectionSource::GetHeight() const {
    return m_inner ? m_inner->GetHeight() : 0;
}

const char* DetectionSource::GetTitle() const {
    return m_inner ? m_inner->GetTitle() : "DetectionSource";
}

double DetectionSource::GetFrameDuration() const {
    return m_inner ? m_inner->GetFrameDuration() : 1.0 / 30.0;
}

RenderDescriptor DetectionSource::GetRenderDescriptor(nv::VideoQuad* vq) const {
    return m_inner ? m_inner->GetRenderDescriptor(vq) : RenderDescriptor{};
}
