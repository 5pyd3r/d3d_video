#include <gtest/gtest.h>
#include <memory>
#include <d3d11.h>

#include "../../src/source/IVideoSource.h"
#include "../../src/detect/IDetector.h"
#include "../../src/detect/IAction.h"
#include "../../src/detect/VideoProcSource.h"
#include "../../src/detect/NullDetector.h"
#include "../../src/detect/NoopAction.h"

namespace nv { class VideoQuad; }

namespace {

class StubSource : public IVideoSource {
public:
    bool Init() override { return true; }
    FrameResult ReadFrame(VideoFrame& out, ID3D11DeviceContext*, nv::VideoQuad*) override {
        m_readCount++;
        out.type = SourceType::File;
        out.width = 1920;
        out.height = 1080;
        return m_result;
    }
    void Close() override {}
    int GetWidth() const override  { return 1920; }
    int GetHeight() const override { return 1080; }
    const char* GetTitle() const override { return "Stub"; }
    double GetFrameDuration() const override { return 1.0 / 30.0; }
    RenderDescriptor GetRenderDescriptor(nv::VideoQuad*) const override { return {}; }

    FrameResult m_result = FrameResult::Got;
    int m_readCount = 0;
};

class AlwaysDetector : public IDetector {
public:
    DetectionResult Detect(const VideoFrame&, ID3D11DeviceContext*) override {
        m_detectCount++;
        return {true, "always"};
    }
    const char* Name() const override { return "AlwaysDetector"; }
    int m_detectCount = 0;
};

class SpyAction : public IAction {
public:
    void OnDetection(const DetectionResult& result, const VideoFrame& frame) override {
        m_invokeCount++;
        m_lastLabel = result.label;
        m_lastWidth = frame.width;
        m_lastHeight = frame.height;
    }
    const char* Name() const override { return "SpyAction"; }
    int m_invokeCount = 0;
    const char* m_lastLabel = nullptr;
    int m_lastWidth = 0;
    int m_lastHeight = 0;
};

} // namespace

// --- Transparent passthrough ---

TEST(VideoProcSourceTest, Passthrough_ReadFrame_DelegatesToInner) {
    auto inner = std::make_unique<StubSource>();
    StubSource* raw = inner.get();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));

    VideoFrame frame;
    FrameResult r = ds->ReadFrame(frame, nullptr, nullptr);
    EXPECT_EQ(r, FrameResult::Got);
    EXPECT_EQ(raw->m_readCount, 1);
    EXPECT_EQ(frame.width, 1920);
    EXPECT_EQ(frame.height, 1080);
}

TEST(VideoProcSourceTest, Passthrough_GetWidth_DelegatesToInner) {
    auto inner = std::make_unique<StubSource>();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));
    EXPECT_EQ(ds->GetWidth(), 1920);
}

TEST(VideoProcSourceTest, Passthrough_GetHeight_DelegatesToInner) {
    auto inner = std::make_unique<StubSource>();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));
    EXPECT_EQ(ds->GetHeight(), 1080);
}

TEST(VideoProcSourceTest, Passthrough_GetTitle_DelegatesToInner) {
    auto inner = std::make_unique<StubSource>();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));
    EXPECT_STREQ(ds->GetTitle(), "Stub");
}

TEST(VideoProcSourceTest, Passthrough_GetFrameDuration_DelegatesToInner) {
    auto inner = std::make_unique<StubSource>();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));
    EXPECT_DOUBLE_EQ(ds->GetFrameDuration(), 1.0 / 30.0);
}

TEST(VideoProcSourceTest, Passthrough_Init_DelegatesToInner) {
    auto inner = std::make_unique<StubSource>();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));
    EXPECT_TRUE(ds->Init());
}

// --- NotReady / End passthrough (no detection run) ---

TEST(VideoProcSourceTest, NotReady_Passthrough_NoDetection) {
    auto inner = std::make_unique<StubSource>();
    inner->m_result = FrameResult::NotReady;
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));

    auto detector = std::make_unique<AlwaysDetector>();
    AlwaysDetector* dRaw = detector.get();
    ds->AddDetector(std::move(detector));

    VideoFrame frame;
    FrameResult r = ds->ReadFrame(frame, nullptr, nullptr);
    EXPECT_EQ(r, FrameResult::NotReady);
    EXPECT_EQ(dRaw->m_detectCount, 0);
}

TEST(VideoProcSourceTest, End_Passthrough_NoDetection) {
    auto inner = std::make_unique<StubSource>();
    inner->m_result = FrameResult::End;
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));

    auto detector = std::make_unique<AlwaysDetector>();
    AlwaysDetector* dRaw = detector.get();
    ds->AddDetector(std::move(detector));

    VideoFrame frame;
    FrameResult r = ds->ReadFrame(frame, nullptr, nullptr);
    EXPECT_EQ(r, FrameResult::End);
    EXPECT_EQ(dRaw->m_detectCount, 0);
}

// --- Detection triggers action ---

TEST(VideoProcSourceTest, Detection_Triggers_Action) {
    auto inner = std::make_unique<StubSource>();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));

    auto detector = std::make_unique<AlwaysDetector>();
    ds->AddDetector(std::move(detector));

    auto action = std::make_unique<SpyAction>();
    SpyAction* aRaw = action.get();
    ds->AddAction(std::move(action));

    VideoFrame frame;
    ds->ReadFrame(frame, nullptr, nullptr);

    EXPECT_EQ(aRaw->m_invokeCount, 1);
    EXPECT_STREQ(aRaw->m_lastLabel, "always");
    EXPECT_EQ(aRaw->m_lastWidth, 1920);
    EXPECT_EQ(aRaw->m_lastHeight, 1080);
}

// --- Multiple detectors, all run ---

TEST(VideoProcSourceTest, MultipleDetectors_AllRun) {
    auto inner = std::make_unique<StubSource>();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));

    auto d1 = std::make_unique<AlwaysDetector>();
    AlwaysDetector* d1Raw = d1.get();
    ds->AddDetector(std::move(d1));

    auto d2 = std::make_unique<AlwaysDetector>();
    AlwaysDetector* d2Raw = d2.get();
    ds->AddDetector(std::move(d2));

    VideoFrame frame;
    ds->ReadFrame(frame, nullptr, nullptr);

    EXPECT_EQ(d1Raw->m_detectCount, 1);
    EXPECT_EQ(d2Raw->m_detectCount, 1);
}

// --- Multiple actions, all fire ---

TEST(VideoProcSourceTest, MultipleActions_AllFire) {
    auto inner = std::make_unique<StubSource>();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));
    ds->AddDetector(std::make_unique<AlwaysDetector>());

    auto a1 = std::make_unique<SpyAction>();
    SpyAction* a1Raw = a1.get();
    ds->AddAction(std::move(a1));

    auto a2 = std::make_unique<SpyAction>();
    SpyAction* a2Raw = a2.get();
    ds->AddAction(std::move(a2));

    VideoFrame frame;
    ds->ReadFrame(frame, nullptr, nullptr);

    EXPECT_EQ(a1Raw->m_invokeCount, 1);
    EXPECT_EQ(a2Raw->m_invokeCount, 1);
}

// --- Empty detector/action list: transparent passthrough ---

TEST(VideoProcSourceTest, NoDetectors_ActsAsPassthrough) {
    auto inner = std::make_unique<StubSource>();
    StubSource* raw = inner.get();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));

    VideoFrame frame;
    FrameResult r = ds->ReadFrame(frame, nullptr, nullptr);

    EXPECT_EQ(r, FrameResult::Got);
    EXPECT_EQ(raw->m_readCount, 1);
}

// --- Detection not triggered when detector returns false ---

TEST(VideoProcSourceTest, NoDetection_ActionNotCalled) {
    auto inner = std::make_unique<StubSource>();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));
    ds->AddDetector(std::make_unique<NullDetector>());

    auto action = std::make_unique<SpyAction>();
    SpyAction* aRaw = action.get();
    ds->AddAction(std::move(action));

    VideoFrame frame;
    ds->ReadFrame(frame, nullptr, nullptr);

    EXPECT_EQ(aRaw->m_invokeCount, 0);
}

// --- GetRenderDescriptor delegates ---

TEST(VideoProcSourceTest, GetRenderDescriptor_DelegatesToInner) {
    auto inner = std::make_unique<StubSource>();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));
    RenderDescriptor rd = ds->GetRenderDescriptor(nullptr);
    EXPECT_EQ(rd.pixelShader, nullptr);
    EXPECT_EQ(rd.srvs[0], nullptr);
    EXPECT_EQ(rd.srvs[1], nullptr);
}

// --- ToggleFilter ---

TEST(VideoProcSourceTest, ToggleFilter_CyclesThroughFilters) {
    auto inner = std::make_unique<StubSource>();
    auto ds = std::make_unique<VideoProcSource>(std::move(inner));
    // No filters added: ToggleFilter should not crash
    EXPECT_FALSE(ds->IsFilterEnabled());
    ds->ToggleFilter();
    EXPECT_FALSE(ds->IsFilterEnabled());
}
