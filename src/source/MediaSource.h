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

    // Outcome of a read attempt. EndOfStream and Error both leave no packet, but
    // they are different conditions: an I/O error must not be reported to the
    // user as the end of the file.
    enum class PacketStatus { Got, EndOfStream, Error };

    uint32_t Open(const char* filePath);

    // Fills `out` (an allocated AVPacket owned by the caller) and reports the
    // outcome. The packet is reused across calls instead of being allocated and
    // freed for every frame.
    PacketStatus ReadPacket(AVPacket* out);

    void Close();

    AVFormatContext* GetFormatContext() const { return fmtCtx; }

private:
    AVFormatContext* fmtCtx;
};

#endif
