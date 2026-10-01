#include "MediaSource.h"
#include "../platform/Logger.h"

extern "C" {
#include <libavformat/avformat.h>
}

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
    // A failure here only degrades the stream metadata (frame rate, duration);
    // playback can still proceed, so it is reported but not treated as fatal.
    int infoErr = avformat_find_stream_info(fmtCtx, NULL);
    if (infoErr < 0) {
        char infoErrStr[256];
        av_make_error_string(infoErrStr, sizeof(infoErrStr), infoErr);
        logger->warn("avformat_find_stream_info failed: {}", infoErrStr);
    }
    return 0;
}

MediaSource::PacketStatus MediaSource::ReadPacket(AVPacket* out) {
    if (!fmtCtx || !out) return PacketStatus::Error;

    av_packet_unref(out);
    int ret = av_read_frame(fmtCtx, out);
    if (ret >= 0) return PacketStatus::Got;

    if (ret == AVERROR_EOF) return PacketStatus::EndOfStream;

    char errStr[256];
    av_make_error_string(errStr, sizeof(errStr), ret);
    logger->warn("av_read_frame failed: {}", errStr);
    return PacketStatus::Error;
}

void MediaSource::Close() {
    if (fmtCtx) {
        avformat_close_input(&fmtCtx);
        fmtCtx = nullptr;
    }
}
