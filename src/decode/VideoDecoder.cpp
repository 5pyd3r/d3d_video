#include "VideoDecoder.h"
#include "../platform/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

VideoDecoder::VideoDecoder() {}
VideoDecoder::~VideoDecoder() { Close(); }

uint32_t VideoDecoder::Init(AVFormatContext* fmtCtx, double& avg_frame_rate,
                            ID3D11Device* device) {
    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
        auto* theStream = fmtCtx->streams[i];
        const AVCodec* codec = avcodec_find_decoder(theStream->codecpar->codec_id);
        if (!codec) continue;

        if (codec->type == AVMEDIA_TYPE_VIDEO) {
            if (theStream->avg_frame_rate.num > 0 && theStream->avg_frame_rate.den > 0) {
                avg_frame_rate = (double)theStream->avg_frame_rate.num / theStream->avg_frame_rate.den;
            } else if (theStream->r_frame_rate.num > 0 && theStream->r_frame_rate.den > 0) {
                avg_frame_rate = (double)theStream->r_frame_rate.num / theStream->r_frame_rate.den;
            } else {
                avg_frame_rate = 30.0;
            }
            auto* vcodecCtx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(vcodecCtx, theStream->codecpar);
            avcodec_open2(vcodecCtx, codec, NULL);
            codecMap[i] = vcodecCtx;

            AVBufferRef* hw_device_ctx = nullptr;
            if (device) {
                hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
                if (hw_device_ctx) {
                    auto* device_ctx = (AVHWDeviceContext*)hw_device_ctx->data;
                    auto* d3d11_ctx = (AVD3D11VADeviceContext*)device_ctx->hwctx;
                    d3d11_ctx->device = device;
                    if (av_hwdevice_ctx_init(hw_device_ctx) < 0) {
                        av_buffer_unref(&hw_device_ctx);
                        hw_device_ctx = nullptr;
                    }
                }
            } else {
                av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_D3D11VA,
                                       NULL, NULL, 0);
            }
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

VideoDecoder::DecodedFrame VideoDecoder::Flush(int streamIndex) {
    auto it = codecMap.find(streamIndex);
    if (it == codecMap.end()) {
        return {AVMEDIA_TYPE_UNKNOWN, nullptr};
    }

    AVCodecContext* codecCtx = it->second;
    avcodec_send_packet(codecCtx, nullptr);

    AVFrame* frame = av_frame_alloc();
    int ret = avcodec_receive_frame(codecCtx, frame);
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
