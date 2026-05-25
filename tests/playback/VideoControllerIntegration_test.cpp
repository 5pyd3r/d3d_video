#include <gtest/gtest.h>
#include <windows.h>
#include <d3d11.h>
#include <memory>
#include <chrono>
#include <thread>

#include "../../src/playback/VideoController.h"
#include "../../src/source/IVideoSource.h"

namespace nv { class VideoQuad; }

namespace {

// Configurable source for integration testing
class TestSource : public IVideoSource {
public:
    TestSource(SourceType type, double frameDuration = 1.0 / 30.0)
        : m_type(type), m_frameDuration(frameDuration) {}

    bool Init() override { return m_initResult; }
    bool ReadFrame(VideoFrame& out, ID3D11DeviceContext*, nv::VideoQuad*) override {
        m_readFrameCount++;
        out.type = m_type;
        out.width = 1920;
        out.height = 1080;
        return m_readFrameResult;
    }
    void Close() override {}
    SourceType GetType() const override { return m_type; }
    int GetWidth() const override { return 1920; }
    int GetHeight() const override { return 1080; }
    const char* GetTitle() const override { return "TestSource"; }
    double GetFrameDuration() const override { return m_frameDuration; }

    bool m_initResult = true;
    bool m_readFrameResult = true;
    int m_readFrameCount = 0;
    SourceType m_type;
    double m_frameDuration;
};

class VideoControllerIntegrationTest : public ::testing::Test {
protected:
    HWND m_hwnd = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_ctx = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    std::unique_ptr<VideoController> m_vc;

    void SetUp() override {
        // Hidden window for swap chain
        WNDCLASSW wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"VCIntegrationTest";
        RegisterClassW(&wc);
        m_hwnd = CreateWindowExW(0, L"VCIntegrationTest", L"", WS_POPUP,
                                  0, 0, 640, 480, nullptr, nullptr, wc.hInstance, nullptr);

        if (!m_hwnd) {
            GTEST_SKIP() << "Cannot create test window";
            return;
        }

        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferDesc.Width = 640;
        scd.BufferDesc.Height = 480;
        scd.BufferDesc.RefreshRate.Numerator = 60;
        scd.BufferDesc.RefreshRate.Denominator = 1;
        scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        scd.SampleDesc.Count = 1;
        scd.SampleDesc.Quality = 0;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.BufferCount = 2;
        scd.OutputWindow = m_hwnd;
        scd.Windowed = TRUE;
        scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        scd.Flags = 0;

        D3D_FEATURE_LEVEL level;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
            D3D11_SDK_VERSION, &scd, &m_swapChain, &m_device, &level, &m_ctx);

        if (FAILED(hr) || !m_device || !m_swapChain) {
            GTEST_SKIP() << "D3D11 device not available on this CI runner";
            return;
        }

        m_vc = std::make_unique<VideoController>();
        m_vc->Init(m_device, m_ctx, m_swapChain, 640, 480);
    }

    void TearDown() override {
        m_vc.reset();
        if (m_ctx) { m_ctx->Release(); m_ctx = nullptr; }
        if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
        if (m_device) { m_device->Release(); m_device = nullptr; }
        if (m_hwnd) { DestroyWindow(m_hwnd); m_hwnd = nullptr; }
    }
};

} // namespace

// --- Initial state ---

TEST_F(VideoControllerIntegrationTest, InitialState_IsStop) {
    if (!m_vc) GTEST_SKIP() << "D3D11 not available";
    EXPECT_EQ(m_vc->GetState(), PlayState::Stop);
}

TEST_F(VideoControllerIntegrationTest, NoSource_RenderReturnsZeroAndDoesNotCrash) {
    if (!m_vc) GTEST_SKIP() << "D3D11 not available";
    EXPECT_EQ(m_vc->Render(m_hwnd), 0u);
    EXPECT_EQ(m_vc->GetState(), PlayState::Stop);
}

// --- File source lifecycle ---

TEST_F(VideoControllerIntegrationTest, SetFileSource_TransitionsToPlay) {
    if (!m_vc) GTEST_SKIP() << "D3D11 not available";
    auto src = std::make_unique<TestSource>(SourceType::File);
    m_vc->SetSource(std::move(src));
    EXPECT_EQ(m_vc->GetState(), PlayState::Play);
    EXPECT_NE(m_vc->GetSource(), nullptr);
}

TEST_F(VideoControllerIntegrationTest, FileSource_EOF_TransitionsToStop) {
    if (!m_vc) GTEST_SKIP() << "D3D11 not available";
    auto src = std::make_unique<TestSource>(SourceType::File);
    src->m_readFrameResult = false;
    m_vc->SetSource(std::move(src));
    EXPECT_EQ(m_vc->GetState(), PlayState::Play);

    // First Render will try ReadFrame, get false, stop
    m_vc->Render(m_hwnd);
    EXPECT_EQ(m_vc->GetState(), PlayState::Stop);
}

// --- Capture source lifecycle ---

TEST_F(VideoControllerIntegrationTest, SetCaptureSource_TransitionsToPlay) {
    if (!m_vc) GTEST_SKIP() << "D3D11 not available";
    auto src = std::make_unique<TestSource>(SourceType::Capture);
    m_vc->SetSource(std::move(src));
    EXPECT_EQ(m_vc->GetState(), PlayState::Play);
}

TEST_F(VideoControllerIntegrationTest, CaptureSource_NoFrame_StaysInPlay) {
    if (!m_vc) GTEST_SKIP() << "D3D11 not available";
    auto src = std::make_unique<TestSource>(SourceType::Capture);
    src->m_readFrameResult = false;  // Simulate no frame ready
    m_vc->SetSource(std::move(src));

    // Multiple Render calls — should NOT transition to Stop
    for (int i = 0; i < 5; i++) {
        m_vc->Render(m_hwnd);
        EXPECT_EQ(m_vc->GetState(), PlayState::Play)
            << "Capture should remain in Play even when ReadFrame returns false";
    }
}

// --- Frame duration ---

TEST_F(VideoControllerIntegrationTest, FrameDuration_RespectedByRender) {
    if (!m_vc) GTEST_SKIP() << "D3D11 not available";
    auto src = std::make_unique<TestSource>(SourceType::File, 1.0 / 60.0);
    m_vc->SetSource(std::move(src));

    // First Render should call ReadFrame (t=0)
    m_vc->Render(m_hwnd);

    auto* ts = static_cast<TestSource*>(m_vc->GetSource());
    int countAfterFirst = ts->m_readFrameCount;
    EXPECT_EQ(countAfterFirst, 1);

    // Immediate second Render should NOT call ReadFrame (not enough time elapsed)
    m_vc->Render(m_hwnd);
    EXPECT_EQ(ts->m_readFrameCount, countAfterFirst)
        << "Should not decode new frame before frame duration elapses";
}

// --- StopSource ---

TEST_F(VideoControllerIntegrationTest, StopSource_ReturnsToStop) {
    if (!m_vc) GTEST_SKIP() << "D3D11 not available";
    m_vc->SetSource(std::make_unique<TestSource>(SourceType::File));
    EXPECT_EQ(m_vc->GetState(), PlayState::Play);

    m_vc->StopSource();
    EXPECT_EQ(m_vc->GetState(), PlayState::Stop);
    EXPECT_EQ(m_vc->GetSource(), nullptr);
}

// --- Render without source ---

TEST_F(VideoControllerIntegrationTest, Render_AfterStopSource_DoesNotCrash) {
    if (!m_vc) GTEST_SKIP() << "D3D11 not available";
    m_vc->SetSource(std::make_unique<TestSource>(SourceType::File));
    m_vc->StopSource();
    EXPECT_EQ(m_vc->Render(m_hwnd), 0u);
}

// --- Multiple sources in sequence ---

TEST_F(VideoControllerIntegrationTest, SwitchSource_FileToCapture) {
    if (!m_vc) GTEST_SKIP() << "D3D11 not available";

    // Start with File
    m_vc->SetSource(std::make_unique<TestSource>(SourceType::File));
    EXPECT_EQ(m_vc->GetState(), PlayState::Play);
    m_vc->Render(m_hwnd);

    // Switch to Capture without explicit Stop
    auto capSrc = std::make_unique<TestSource>(SourceType::Capture);
    capSrc->m_readFrameResult = false;
    m_vc->SetSource(std::move(capSrc));
    EXPECT_EQ(m_vc->GetState(), PlayState::Play);

    // Should stay in Play (capture semantics)
    m_vc->Render(m_hwnd);
    EXPECT_EQ(m_vc->GetState(), PlayState::Play);
}

// --- GetSource returns correct pointer ---

TEST_F(VideoControllerIntegrationTest, GetSource_ReturnsNonNull_WhenSourceSet) {
    if (!m_vc) GTEST_SKIP() << "D3D11 not available";
    EXPECT_EQ(m_vc->GetSource(), nullptr);
    m_vc->SetSource(std::make_unique<TestSource>(SourceType::File));
    EXPECT_NE(m_vc->GetSource(), nullptr);
}
