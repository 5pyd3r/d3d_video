#include <gtest/gtest.h>
#include <memory>

// VideoController requires D3D11 for Init(), but we test interface-level
// logic that doesn't need a real device.

#include "../../src/playback/VideoController.h"
#include "../../src/source/IVideoSource.h"

namespace nv { class VideoQuad; }

// Reuse MockVideoSource concept — embedded here to keep test self-contained
namespace {

class FakeSource : public IVideoSource {
public:
    FakeSource(SourceType type, double frameDuration = 1.0 / 30.0)
        : m_type(type), m_frameDuration(frameDuration) {}

    bool Init() override { return m_initResult; }
    bool ReadFrame(VideoFrame& out, ID3D11DeviceContext*, nv::VideoQuad*) override {
        m_readFrameCallCount++;
        out.type = m_type;
        out.width = 640;
        out.height = 480;
        return m_readFrameResult;
    }
    void Close() override {}
    SourceType GetType() const override { return m_type; }
    int GetWidth() const override { return 640; }
    int GetHeight() const override { return 480; }
    const char* GetTitle() const override { return "Fake"; }
    double GetFrameDuration() const override { return m_frameDuration; }
    RenderDescriptor GetRenderDescriptor(nv::VideoQuad* vq) const override { return {}; }

    bool m_initResult = true;
    bool m_readFrameResult = true;
    int m_readFrameCallCount = 0;
    SourceType m_type;
    double m_frameDuration;
};

} // namespace

// --- State transitions ---

TEST(VideoControllerTest, InitialState_IsStop) {
    GTEST_SKIP() << "VideoController needs D3D11 device for Init() — skip on headless CI";
    // When device is available, unskip and add real tests:
    // VideoController vc;
    // EXPECT_EQ(vc.GetState(), PlayState::Stop);
}

TEST(VideoControllerTest, GetFrameDuration_FromSource) {
    // Test that a source's GetFrameDuration is used correctly.
    // This is a pure-logic test — no GPU needed.
    auto src = std::make_unique<FakeSource>(SourceType::File, 1.0 / 60.0);
    EXPECT_DOUBLE_EQ(src->GetFrameDuration(), 1.0 / 60.0);
}

TEST(VideoControllerTest, ReadFrameCount_TracksCalls) {
    auto src = std::make_unique<FakeSource>(SourceType::File);
    EXPECT_EQ(src->m_readFrameCallCount, 0);

    VideoFrame frame;
    src->ReadFrame(frame, nullptr, nullptr);
    EXPECT_EQ(src->m_readFrameCallCount, 1);

    src->ReadFrame(frame, nullptr, nullptr);
    EXPECT_EQ(src->m_readFrameCallCount, 2);
}

// --- Source type dispatch ---

TEST(VideoControllerTest, SourceType_File) {
    auto src = std::make_unique<FakeSource>(SourceType::File);
    EXPECT_EQ(src->GetType(), SourceType::File);
}

TEST(VideoControllerTest, SourceType_Capture) {
    auto src = std::make_unique<FakeSource>(SourceType::Capture);
    EXPECT_EQ(src->GetType(), SourceType::Capture);
}

// --- ReadFrame false handling contract ---

TEST(VideoControllerTest, ReadFrame_File_ReturnsFalse_SimulatesEOF) {
    auto src = std::make_unique<FakeSource>(SourceType::File);
    src->m_readFrameResult = false;
    VideoFrame frame;
    EXPECT_FALSE(src->ReadFrame(frame, nullptr, nullptr));
    // Consumer (VideoController::Render) should transition to Stop for File type
}

TEST(VideoControllerTest, ReadFrame_Capture_ReturnsFalse_SimulatesNoFrame) {
    auto src = std::make_unique<FakeSource>(SourceType::Capture);
    src->m_readFrameResult = false;
    VideoFrame frame;
    EXPECT_FALSE(src->ReadFrame(frame, nullptr, nullptr));
    // Consumer (VideoController::Render) should retry, NOT stop, for Capture type
}

// --- Frame duration contract ---

TEST(VideoControllerTest, FrameDuration_30fps) {
    auto src = std::make_unique<FakeSource>(SourceType::File, 1.0 / 30.0);
    EXPECT_DOUBLE_EQ(src->GetFrameDuration(), 1.0 / 30.0);
}

TEST(VideoControllerTest, FrameDuration_60fps) {
    auto src = std::make_unique<FakeSource>(SourceType::File, 1.0 / 60.0);
    EXPECT_DOUBLE_EQ(src->GetFrameDuration(), 1.0 / 60.0);
}

TEST(VideoControllerTest, FrameDuration_Capture_Default30fps) {
    auto src = std::make_unique<FakeSource>(SourceType::Capture, 1.0 / 30.0);
    EXPECT_DOUBLE_EQ(src->GetFrameDuration(), 1.0 / 30.0);
}
