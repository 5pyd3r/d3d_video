#include "MediaSource.h"
#include "../platform/Logger.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <spdlog/spdlog.h>

extern std::shared_ptr<spdlog::logger> logger;

MediaSource::MediaSource() : fmtCtx(nullptr) {}

MediaSource::~MediaSource() {
    Close();
}

uint32_t MediaSource::Open(const char* filePath) {
    int err = avformat_open_input(&fmtCtx, filePath, NULL, NULL);
    if (fmtCtx == nullptr) {
        char errStr[256];
        av_make_error_string(errStr, sizeof(errStr), err);
        logger->error("avformat_open_input failed: {}, file: {}", errStr, filePath);
        return 1;
    }
    avformat_find_stream_info(fmtCtx, NULL);
    return 0;
}

AVPacket* MediaSource::ReadPacket() {
    AVPacket* packet = av_packet_alloc();
    int ret = av_read_frame(fmtCtx, packet);
    if (ret < 0) {
        av_packet_free(&packet);
        return nullptr;
    }
    return packet;
}

void MediaSource::Close() {
    if (fmtCtx) {
        avformat_close_input(&fmtCtx);
        fmtCtx = nullptr;
    }
}
