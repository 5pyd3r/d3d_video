#include "VideoDecoder.h"
#include "../platform/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
}

#include <spdlog/spdlog.h>

extern std::shared_ptr<spdlog::logger> logger;

VideoDecoder::VideoDecoder() {}
VideoDecoder::~VideoDecoder() { Close(); }

uint32_t VideoDecoder::Init(AVFormatContext* fmtCtx, double& avg_frame_rate) {
    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
        auto* theStream = fmtCtx->streams[i];
        const AVCodec* codec = avcodec_find_decoder(theStream->codecpar->codec_id);
        if (!codec) continue;

        if (codec->type == AVMEDIA_TYPE_VIDEO) {
            avg_frame_rate = (double)theStream->avg_frame_rate.num / theStream->avg_frame_rate.den;
            auto* vcodecCtx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(vcodecCtx, theStream->codecpar);
            avcodec_open2(vcodecCtx, codec, NULL);
            codecMap[i] = vcodecCtx;

            AVBufferRef* hw_device_ctx = nullptr;
            av_hwdevice_ctx_create(&hw_device_ctx, AVHWDeviceType::AV_HWDEVICE_TYPE_D3D11VA, NULL, NULL, NULL);
            if (hw_device_ctx) {
                vcodecCtx->hw_device_ctx = hw_device_ctx;
            }
        } else if (codec->type == AVMEDIA_TYPE_AUDIO) {
            auto* acodecCtx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(acodecCtx, fmtCtx->streams[i]->codecpar);
            avcodec_open2(acodecCtx, codec, NULL);
            codecMap[i] = acodecCtx;
        }
    }
    return 0;
}

VideoDecoder::DecodedFrame VideoDecoder::SendAndReceive(AVPacket* packet) {
    if (!packet) return {AVMEDIA_TYPE_UNKNOWN, nullptr};

    auto it = codecMap.find(packet->stream_index);
    if (it == codecMap.end()) {
        return {AVMEDIA_TYPE_UNKNOWN, nullptr};
    }

    AVCodecContext* codecCtx = it->second;
    int ret = avcodec_send_packet(codecCtx, packet);
    if (ret != 0) return {AVMEDIA_TYPE_UNKNOWN, nullptr};

    AVFrame* frame = av_frame_alloc();
    ret = avcodec_receive_frame(codecCtx, frame);
    if (ret == 0) {
        return {codecCtx->codec_type, frame};
    }
    av_frame_free(&frame);
    return {AVMEDIA_TYPE_UNKNOWN, nullptr};
}

void VideoDecoder::Close() {
    for (auto& it : codecMap) {
        avcodec_free_context(&it.second);
    }
    codecMap.clear();
}
