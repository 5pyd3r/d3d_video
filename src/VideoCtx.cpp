#include "VideoCtx.h"
#include "source/MediaSource.h"
#include "platform/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

#include <spdlog/spdlog.h>

extern std::shared_ptr<spdlog::logger> logger;

VideoCtx::VideoCtx() {
    source = new MediaSource();
}

VideoCtx::~VideoCtx() {
    Deinit();
    delete source;
}

uint32_t VideoCtx::Init(const char* filePath) {
    return source->Open(filePath);
}

uint32_t VideoCtx::InitCodec(double& avg_frame_rate) {
    AVCodecContext* vcodecCtx = nullptr;
    AVCodecContext* acodecCtx = nullptr;
    auto* fmtCtx = source->GetFormatContext();

    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
        auto* theStream = fmtCtx->streams[i];
        const AVCodec* codec = avcodec_find_decoder(theStream->codecpar->codec_id);
        if (codec->type == AVMEDIA_TYPE_VIDEO) {
            avg_frame_rate = (double)theStream->avg_frame_rate.den / theStream->avg_frame_rate.num;
            vcodecCtx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(vcodecCtx, theStream->codecpar);
            avcodec_open2(vcodecCtx, codec, NULL);
            codecMap[i] = vcodecCtx;

            AVBufferRef* hw_device_ctx = nullptr;
            av_hwdevice_ctx_create(&hw_device_ctx, AVHWDeviceType::AV_HWDEVICE_TYPE_D3D11VA, NULL, NULL, NULL);
            if (hw_device_ctx) {
                vcodecCtx->hw_device_ctx = hw_device_ctx;
            }
        } else if (codec->type == AVMEDIA_TYPE_AUDIO) {
            acodecCtx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(acodecCtx, fmtCtx->streams[i]->codecpar);
            avcodec_open2(acodecCtx, codec, NULL);
            codecMap[i] = acodecCtx;
        }
    }
    return 0;
}

uint32_t VideoCtx::Deinit() {
    for (auto& it : codecMap) {
        avcodec_free_context(&it.second);
    }
    codecMap.clear();
    source->Close();
    return 0;
}

uint32_t VideoCtx::Reinit(const char* filePath, double& avg_frame_rate) {
    Deinit();
    uint32_t ret = Init(filePath);
    if (ret != 0) return ret;
    return InitCodec(avg_frame_rate);
}

VideoCtx::MediaFrame VideoCtx::nextFrame() {
    AVPacket* packet = source->ReadPacket();
    if (!packet) {
        return {AVMEDIA_TYPE_UNKNOWN, nullptr};
    }

    auto it = codecMap.find(packet->stream_index);
    if (it == codecMap.end()) {
        av_packet_free(&packet);
        return {AVMEDIA_TYPE_UNKNOWN, nullptr};
    }

    AVCodecContext* codecCtx = it->second;
    int ret = avcodec_send_packet(codecCtx, packet);
    av_packet_free(&packet);

    if (ret != 0) {
        return {AVMEDIA_TYPE_UNKNOWN, nullptr};
    }

    AVFrame* frame = av_frame_alloc();
    ret = avcodec_receive_frame(codecCtx, frame);
    if (ret == 0) {
        return {codecCtx->codec_type, frame};
    }

    av_frame_free(&frame);
    return {AVMEDIA_TYPE_UNKNOWN, nullptr};
}
