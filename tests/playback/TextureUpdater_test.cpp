#include <gtest/gtest.h>

#include <windows.h>
#include <d3d11.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

#include "../../src/playback/TextureUpdater.h"
#include "../../src/render/VideoQuad.h"

// IsD3D11Frame() is the gate every caller uses before treating a decoded frame
// as a GPU texture. These cases pin that contract without needing a device.

TEST(TextureUpdaterFrameGateTest, NullFrameIsRejected) {
    EXPECT_FALSE(TextureUpdater::IsD3D11Frame(nullptr));
}

TEST(TextureUpdaterFrameGateTest, SoftwareFrameIsRejected) {
    AVFrame* frame = av_frame_alloc();
    ASSERT_NE(frame, nullptr);

    frame->format = AV_PIX_FMT_YUV420P;
    frame->data[0] = reinterpret_cast<uint8_t*>(0x1);  // CPU buffer, not a texture
    EXPECT_FALSE(TextureUpdater::IsD3D11Frame(frame));

    frame->format = AV_PIX_FMT_NV12;
    EXPECT_FALSE(TextureUpdater::IsD3D11Frame(frame));

    av_frame_free(&frame);
}

TEST(TextureUpdaterFrameGateTest, HardwareFrameWithoutTextureIsRejected) {
    AVFrame* frame = av_frame_alloc();
    ASSERT_NE(frame, nullptr);

    frame->format = AV_PIX_FMT_D3D11;
    frame->data[0] = nullptr;
    EXPECT_FALSE(TextureUpdater::IsD3D11Frame(frame));

    av_frame_free(&frame);
}

TEST(TextureUpdaterFrameGateTest, HardwareFrameWithTextureIsAccepted) {
    AVFrame* frame = av_frame_alloc();
    ASSERT_NE(frame, nullptr);

    frame->format = AV_PIX_FMT_D3D11;
    frame->data[0] = reinterpret_cast<uint8_t*>(0x1);  // ID3D11Texture2D*
    frame->data[1] = reinterpret_cast<uint8_t*>(0x2);  // subresource index
    EXPECT_TRUE(TextureUpdater::IsD3D11Frame(frame));

    av_frame_free(&frame);
}

namespace {

// Real-device fixture with a WARP fallback, so the guard tests still execute on
// a headless runner instead of skipping.
class VideoQuadGuardTest : public ::testing::Test {
protected:
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_ctx = nullptr;

    void SetUp() override {
        static const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1
        };
        const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;

        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                       levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                       &m_device, &got, &m_ctx);
        if (FAILED(hr)) {
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                                   levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                   &m_device, &got, &m_ctx);
        }
        if (FAILED(hr) || m_device == nullptr) {
            m_device = nullptr;
            m_ctx = nullptr;
        }
    }

    void TearDown() override {
        if (m_ctx) { m_ctx->Release(); m_ctx = nullptr; }
        if (m_device) { m_device->Release(); m_device = nullptr; }
    }
};

}  // namespace

TEST_F(VideoQuadGuardTest, ResizeWithInvalidSizeFailsClosed) {
    if (!m_device) GTEST_SKIP() << "no D3D11 device (hardware or WARP) available";

    nv::VideoQuad vq(m_device, m_ctx, 64, 64);
    ASSERT_NE(vq.GetVideoTexture(), nullptr);
    ASSERT_NE(vq.GetsharedHandle(), nullptr);

    vq.Resize(0, 0);  // D3D11 rejects a zero-sized texture

    EXPECT_EQ(vq.GetsharedHandle(), nullptr);
    EXPECT_EQ(vq.GetLuminanceSRV(), nullptr);
    EXPECT_EQ(vq.GetChrominanceSRV(), nullptr);
}

TEST_F(VideoQuadGuardTest, ResizeAfterFailureCanSucceedAgain) {
    if (!m_device) GTEST_SKIP() << "no D3D11 device (hardware or WARP) available";

    nv::VideoQuad vq(m_device, m_ctx, 64, 64);
    vq.Resize(0, 0);
    ASSERT_EQ(vq.GetsharedHandle(), nullptr);

    vq.Resize(32, 32);

    EXPECT_NE(vq.GetVideoTexture(), nullptr);
    EXPECT_NE(vq.GetsharedHandle(), nullptr);
    EXPECT_NE(vq.GetLuminanceSRV(), nullptr);
}

TEST_F(VideoQuadGuardTest, InitCaptureIsRepeatableAndFailsClosed) {
    if (!m_device) GTEST_SKIP() << "no D3D11 device (hardware or WARP) available";

    nv::VideoQuad vq(m_device, m_ctx, 64, 64);

    vq.InitCapture(32, 32);
    ASSERT_NE(vq.GetCapturePixelShader(), nullptr);
    ASSERT_NE(vq.GetCaptureSRV(), nullptr);

    // Re-picking a capture target must replace the shader, not leak or dangle it.
    vq.InitCapture(32, 32);
    EXPECT_NE(vq.GetCapturePixelShader(), nullptr);
    EXPECT_NE(vq.GetCaptureSRV(), nullptr);

    // An impossible size must leave no stale capture SRV behind.
    vq.InitCapture(0, 0);
    EXPECT_EQ(vq.GetCaptureSRV(), nullptr);
}
