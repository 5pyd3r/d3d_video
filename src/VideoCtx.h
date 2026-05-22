#ifndef VIDEOCTX_H
#define VIDEOCTX_H

#include <cstdint>
#include <map>

struct AVFrame;
struct AVCodecContext;
class MediaSource;

class VideoCtx {
public:
    VideoCtx();
    ~VideoCtx();
    uint32_t Init(const char* filePath);
    uint32_t InitCodec(double& avg_frame_rate);
    uint32_t Deinit();
    uint32_t Reinit(const char* filePath, double& avg_frame_rate);
    struct MediaFrame {
        int type;
        AVFrame* frame;
    };
    MediaFrame nextFrame();

private:
    MediaSource* source;
    std::map<int, AVCodecContext*> codecMap;
};

#endif
