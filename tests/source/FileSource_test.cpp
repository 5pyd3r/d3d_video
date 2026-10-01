#include <gtest/gtest.h>

#include <cstdio>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

#include "../../src/source/FileSource.h"
#include "../../src/source/IVideoSource.h"

namespace {

// Generates an audio-only MP4. It has plenty of packets but no video stream, so
// the decoder can never produce a video frame from it - exactly the shape that
// used to make FileSource::ReadFrame decode the whole file inside one call.
bool CreateAudioOnlyFile(const char* path, int seconds) {
    AVFormatContext* oc = nullptr;
    if (avformat_alloc_output_context2(&oc, nullptr, "mp4", path) < 0 || oc == nullptr) return false;

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        avformat_free_context(oc);
        return false;
    }

    AVStream* stream = avformat_new_stream(oc, nullptr);
    AVCodecContext* cctx = avcodec_alloc_context3(codec);
    if (!stream || !cctx) {
        avcodec_free_context(&cctx);
        avformat_free_context(oc);
        return false;
    }

    cctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    cctx->sample_rate = 48000;
    cctx->bit_rate = 64000;
    av_channel_layout_default(&cctx->ch_layout, 1);
    cctx->time_base = {1, cctx->sample_rate};
    if (oc->oformat->flags & AVFMT_GLOBALHEADER) cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    stream->time_base = cctx->time_base;

    if (avcodec_open2(cctx, codec, nullptr) < 0 ||
        avcodec_parameters_from_context(stream->codecpar, cctx) < 0 ||
        avio_open(&oc->pb, path, AVIO_FLAG_WRITE) < 0 ||
        avformat_write_header(oc, nullptr) < 0) {
        avcodec_free_context(&cctx);
        avformat_free_context(oc);
        return false;
    }

    AVFrame* frame = av_frame_alloc();
    frame->format = cctx->sample_fmt;
    frame->sample_rate = cctx->sample_rate;
    frame->nb_samples = cctx->frame_size > 0 ? cctx->frame_size : 1024;
    av_channel_layout_copy(&frame->ch_layout, &cctx->ch_layout);
    if (av_frame_get_buffer(frame, 0) < 0) {
        av_frame_free(&frame);
        avcodec_free_context(&cctx);
        avio_closep(&oc->pb);
        avformat_free_context(oc);
        return false;
    }

    AVPacket* pkt = av_packet_alloc();
    const int64_t targetSamples = static_cast<int64_t>(cctx->sample_rate) * seconds;
    int64_t written = 0;
    bool ok = true;
    while (ok && written < targetSamples) {
        if (av_frame_make_writable(frame) < 0) { ok = false; break; }
        av_samples_set_silence(frame->data, 0, frame->nb_samples,
                               cctx->ch_layout.nb_channels, cctx->sample_fmt);
        frame->pts = written;
        written += frame->nb_samples;
        if (avcodec_send_frame(cctx, frame) < 0) { ok = false; break; }
        while (avcodec_receive_packet(cctx, pkt) == 0) {
            av_packet_rescale_ts(pkt, cctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            av_interleaved_write_frame(oc, pkt);
            av_packet_unref(pkt);
        }
    }
    if (ok) {
        avcodec_send_frame(cctx, nullptr);
        while (avcodec_receive_packet(cctx, pkt) == 0) {
            av_packet_rescale_ts(pkt, cctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            av_interleaved_write_frame(oc, pkt);
            av_packet_unref(pkt);
        }
        av_write_trailer(oc);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&cctx);
    avio_closep(&oc->pb);
    avformat_free_context(oc);
    return ok;
}

}  // namespace

// A stream whose packets never produce a video frame must not be decoded to EOF
// inside a single ReadFrame() call: the frame loop has to get control back.
TEST(FileSourceTest, AudioOnlyFileYieldsInsteadOfDecodingToEof) {
    const char* path = "test_audio_only.mp4";
    if (!CreateAudioOnlyFile(path, 10)) {
        GTEST_SKIP() << "this FFmpeg build has no AAC encoder / mp4 muxer";
        return;
    }

    // No D3D device is needed: an audio-only file never reaches the texture copy.
    FileSource source(path, nullptr);
    ASSERT_TRUE(source.Init());

    VideoFrame frame = {};
    EXPECT_EQ(source.ReadFrame(frame, nullptr, nullptr), FrameResult::NotReady);

    source.Close();
    std::remove(path);
}
