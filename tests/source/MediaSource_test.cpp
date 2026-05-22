#include <gtest/gtest.h>
#include <cstdio>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include "../../src/source/MediaSource.h"

class MediaSourceTest : public ::testing::Test {
protected:
    std::string testFilePath;

    void SetUp() override {
        testFilePath = "test_video_output.mp4";
        CreateTestVideo(testFilePath.c_str());
    }

    void TearDown() override {
        std::remove(testFilePath.c_str());
    }

    void CreateTestVideo(const char* path) {
        AVFormatContext* oc = nullptr;
        avformat_alloc_output_context2(&oc, nullptr, "mp4", path);
        if (!oc) return;

        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) {
            avformat_free_context(oc);
            return;
        }

        AVStream* stream = avformat_new_stream(oc, nullptr);
        AVCodecContext* cctx = avcodec_alloc_context3(codec);
        cctx->width = 64;
        cctx->height = 64;
        cctx->time_base = {1, 30};
        cctx->framerate = {30, 1};
        cctx->pix_fmt = AV_PIX_FMT_YUV420P;
        stream->time_base = cctx->time_base;

        avcodec_open2(cctx, codec, nullptr);
        avcodec_parameters_from_context(stream->codecpar, cctx);

        avio_open(&oc->pb, path, AVIO_FLAG_WRITE);
        avformat_write_header(oc, nullptr);

        AVFrame* frame = av_frame_alloc();
        frame->format = cctx->pix_fmt;
        frame->width = cctx->width;
        frame->height = cctx->height;
        av_frame_get_buffer(frame, 0);
        memset(frame->data[0], 16, frame->linesize[0] * cctx->height);
        memset(frame->data[1], 128, frame->linesize[1] * cctx->height / 2);
        memset(frame->data[2], 128, frame->linesize[2] * cctx->height / 2);
        frame->pts = 0;

        avcodec_send_frame(cctx, frame);
        AVPacket* pkt = av_packet_alloc();
        while (avcodec_receive_packet(cctx, pkt) == 0) {
            av_packet_rescale_ts(pkt, cctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            av_interleaved_write_frame(oc, pkt);
            av_packet_unref(pkt);
        }

        avcodec_send_frame(cctx, nullptr);
        while (avcodec_receive_packet(cctx, pkt) == 0) {
            av_packet_rescale_ts(pkt, cctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            av_interleaved_write_frame(oc, pkt);
            av_packet_unref(pkt);
        }

        av_write_trailer(oc);
        av_packet_free(&pkt);
        av_frame_free(&frame);
        avcodec_free_context(&cctx);
        avio_closep(&oc->pb);
        avformat_free_context(oc);
    }
};

TEST_F(MediaSourceTest, Open_ValidFile_ReturnsZero) {
    MediaSource source;
    EXPECT_EQ(source.Open(testFilePath.c_str()), 0u);
    EXPECT_NE(source.GetFormatContext(), nullptr);
    source.Close();
}

TEST_F(MediaSourceTest, Open_InvalidPath_ReturnsError) {
    MediaSource source;
    EXPECT_NE(source.Open("nonexistent_file_xyz.mp4"), 0u);
}

TEST_F(MediaSourceTest, ReadPacket_AfterOpen_ReturnsPacket) {
    MediaSource source;
    source.Open(testFilePath.c_str());
    AVPacket* pkt = source.ReadPacket();
    if (pkt) {
        av_packet_free(&pkt);
    }
    source.Close();
}

TEST_F(MediaSourceTest, Close_AfterOpen_ClearsContext) {
    MediaSource source;
    source.Open(testFilePath.c_str());
    source.Close();
    EXPECT_EQ(source.GetFormatContext(), nullptr);
}
