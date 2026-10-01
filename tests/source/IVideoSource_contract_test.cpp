#include <gtest/gtest.h>
#include <memory>
#include <d3d11.h>

#include "../../src/source/IVideoSource.h"

// Contract test for the IVideoSource interface itself: a minimal implementation
// must satisfy every pure virtual, and the frame/result enums must keep their
// meaning. It deliberately exercises only the mock - the concrete sources
// (FileSource, CaptureSource, PlaylistSource, VideoProcSource) are covered by
// their own tests - so it must never be read as playback coverage.
namespace nv { class VideoQuad; }

// Minimal mock: configurable return values for all IVideoSource methods
class MockVideoSource : public IVideoSource {
public:
    MockVideoSource(SourceType type) : m_type(type) {}

    bool Init() override { return m_initResult; }
    FrameResult ReadFrame(VideoFrame& out, ID3D11DeviceContext*, nv::VideoQuad*) override {
        out.type = m_type;
        out.width = m_width;
        out.height = m_height;
        return m_readFrameResult;
    }
    void Close() override {}
    int GetWidth() const override { return m_width; }
    int GetHeight() const override { return m_height; }
    const char* GetTitle() const override { return "Mock"; }
    double GetFrameDuration() const override { return m_frameDuration; }
    RenderDescriptor GetRenderDescriptor(nv::VideoQuad*) const override { return {}; }

    // Configurable behavior
    bool m_initResult = true;
    FrameResult m_readFrameResult = FrameResult::Got;
    int m_width = 640;
    int m_height = 480;
    double m_frameDuration = 1.0 / 60.0;
    SourceType m_type;
};

// --- GetFrameDuration is pure virtual ---

TEST(IVideoSourceContractTest, GetFrameDuration_ReturnsConfiguredValue) {
    MockVideoSource src(SourceType::File);
    src.m_frameDuration = 1.0 / 60.0;
    EXPECT_DOUBLE_EQ(src.GetFrameDuration(), 1.0 / 60.0);
}

TEST(IVideoSourceContractTest, GetFrameDuration_DefaultValue) {
    MockVideoSource src(SourceType::File);
    EXPECT_DOUBLE_EQ(src.GetFrameDuration(), 1.0 / 60.0); // matches m_frameDuration init
}

// --- ReadFrame semantics ---

TEST(IVideoSourceContractTest, ReadFrame_SetsFrameType) {
    MockVideoSource src(SourceType::Capture);
    VideoFrame frame;
    FrameResult ok = src.ReadFrame(frame, nullptr, nullptr);
    EXPECT_EQ(ok, FrameResult::Got);
    EXPECT_EQ(frame.type, SourceType::Capture);
}

TEST(IVideoSourceContractTest, ReadFrame_ReturnsFalseWhenConfigured) {
    MockVideoSource src(SourceType::File);
    src.m_readFrameResult = FrameResult::End;
    VideoFrame frame;
    FrameResult ok = src.ReadFrame(frame, nullptr, nullptr);
    EXPECT_EQ(ok, FrameResult::End);
}

// --- Init semantics ---

TEST(IVideoSourceContractTest, Init_ReturnsConfiguredValue) {
    MockVideoSource src(SourceType::File);
    EXPECT_TRUE(src.Init());
    src.m_initResult = false;
    EXPECT_FALSE(src.Init());
}

// --- GetTitle returns string ---

TEST(IVideoSourceContractTest, GetTitle_NotNull) {
    MockVideoSource src(SourceType::File);
    EXPECT_STREQ(src.GetTitle(), "Mock");
}
