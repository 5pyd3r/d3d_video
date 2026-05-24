#ifndef DECODE_VIDEODECODER_H
#define DECODE_VIDEODECODER_H

#include <cstdint>
#include <map>

struct AVFrame;
struct AVCodecContext;
struct AVFormatContext;
struct AVPacket;
struct ID3D11Device;

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    uint32_t Init(AVFormatContext* fmtCtx, double& avg_frame_rate, ID3D11Device* d3dDevice = nullptr);
    void Close();

    struct DecodedFrame {
        int type;       // AVMEDIA_TYPE_VIDEO or AVMEDIA_TYPE_AUDIO
        AVFrame* frame;
    };
    DecodedFrame SendAndReceive(AVPacket* packet);
    DecodedFrame Flush(int streamIndex);

private:
    std::map<int, AVCodecContext*> codecMap;
};

#endif
