#ifndef VIDEOCTX_H
#define VIDEOCTX_H

#include <cstdint>
#include <map>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

struct MediaFrame {
	AVMediaType type;
	AVFrame* frame;
};

class VideoCtx {
public:
    VideoCtx();
    ~VideoCtx();
    uint32_t Init(const char *filePath);
    uint32_t InitCodec(double &avg_frame_rate);
    uint32_t Deinit();
    uint32_t Reinit(const char *filePath, double &avg_frame_rate);
    MediaFrame nextFrame();

private:
    AVFormatContext *fmtCtx;
	std::map<int, AVCodecContext *> codecMap;
};

#endif