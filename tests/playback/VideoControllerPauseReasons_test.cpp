#include <gtest/gtest.h>

#include <memory>

#include "../../src/playback/VideoController.h"
#include "../../src/source/IVideoSource.h"

namespace nv { class VideoQuad; }

namespace {

// Minimal source: the pause bookkeeping under test lives entirely in
// VideoController, and none of these paths needs a swap chain or GPU device.
class StubSource : public IVideoSource {
public:
    bool Init() override { return true; }
    FrameResult ReadFrame(VideoFrame& out, ID3D11DeviceContext*, nv::VideoQuad*) override {
        out.type = SourceType::File;
        out.width = 640;
        out.height = 480;
        return FrameResult::Got;
    }
    void Close() override {}
    int GetWidth() const override { return 640; }
    int GetHeight() const override { return 480; }
    const char* GetTitle() const override { return "Stub"; }
    double GetFrameDuration() const override { return 1.0 / 30.0; }
    RenderDescriptor GetRenderDescriptor(nv::VideoQuad*) const override { return {}; }
};

}  // namespace

class PauseReasonTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_vc.SetSource(std::make_unique<StubSource>());
        ASSERT_EQ(m_vc.GetState(), PlayState::Play);
    }

    VideoController m_vc;
};

TEST_F(PauseReasonTest, DragPauseAndResumeRoundTrips) {
    m_vc.Pause(PauseReason::WindowDrag);
    EXPECT_EQ(m_vc.GetState(), PlayState::Pause);

    m_vc.Resume(PauseReason::WindowDrag);
    EXPECT_EQ(m_vc.GetState(), PlayState::Play);
}

// The regression this branch exists for: one reason releasing must not cancel a
// pause that another reason is still holding.
TEST_F(PauseReasonTest, OverlappingReasonsResumeOnlyAfterTheLastRelease) {
    m_vc.Pause(PauseReason::WindowDrag);
    m_vc.Pause(PauseReason::SystemSuspend);
    ASSERT_EQ(m_vc.GetState(), PlayState::Pause);

    m_vc.Resume(PauseReason::WindowDrag);
    EXPECT_EQ(m_vc.GetState(), PlayState::Pause)
        << "a drag ending must not restart a suspended player";

    m_vc.Resume(PauseReason::SystemSuspend);
    EXPECT_EQ(m_vc.GetState(), PlayState::Play);
}

TEST_F(PauseReasonTest, SystemResumeDoesNotCancelAnActiveDrag) {
    m_vc.OnSystemSuspend();
    m_vc.Pause(PauseReason::WindowDrag);
    ASSERT_EQ(m_vc.GetState(), PlayState::Pause);

    m_vc.OnSystemResume();
    EXPECT_EQ(m_vc.GetState(), PlayState::Pause)
        << "a resume from suspend must not cancel an active drag";

    m_vc.Resume(PauseReason::WindowDrag);
    EXPECT_EQ(m_vc.GetState(), PlayState::Play);
}

TEST_F(PauseReasonTest, DuplicatePauseAndResumeAreIdempotent) {
    m_vc.Pause(PauseReason::WindowDrag);
    m_vc.Pause(PauseReason::WindowDrag);
    EXPECT_EQ(m_vc.GetState(), PlayState::Pause);

    m_vc.Resume(PauseReason::WindowDrag);
    m_vc.Resume(PauseReason::WindowDrag);
    EXPECT_EQ(m_vc.GetState(), PlayState::Play);
}

TEST_F(PauseReasonTest, ResumeWithoutPauseLeavesPlaybackAlone) {
    m_vc.Resume(PauseReason::SystemSuspend);
    EXPECT_EQ(m_vc.GetState(), PlayState::Play);
}

TEST_F(PauseReasonTest, StopWhilePausedIsNotResurrected) {
    m_vc.Pause(PauseReason::WindowDrag);
    m_vc.StopSource();
    ASSERT_EQ(m_vc.GetState(), PlayState::Stop);

    m_vc.Resume(PauseReason::WindowDrag);
    EXPECT_EQ(m_vc.GetState(), PlayState::Stop);
}

TEST_F(PauseReasonTest, SourceSetWhilePausedStaysPaused) {
    m_vc.Pause(PauseReason::SystemSuspend);

    m_vc.SetSource(std::make_unique<StubSource>());
    EXPECT_EQ(m_vc.GetState(), PlayState::Pause);

    m_vc.Resume(PauseReason::SystemSuspend);
    EXPECT_EQ(m_vc.GetState(), PlayState::Play);
}
