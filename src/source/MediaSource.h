#ifndef SOURCE_MEDIASOURCE_H
#define SOURCE_MEDIASOURCE_H

#include <cstdint>
#include <string>

struct AVFormatContext;
struct AVPacket;

class MediaSource {
public:
    MediaSource();
    ~MediaSource();

    uint32_t Open(const char* filePath);
    AVPacket* ReadPacket();
    void Close();

    AVFormatContext* GetFormatContext() const { return fmtCtx; }

private:
    AVFormatContext* fmtCtx;
};

#endif
