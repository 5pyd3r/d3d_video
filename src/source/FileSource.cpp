#include "FileSource.h"
#include "../platform/Logger.h"
#include "../playback/TextureUpdater.h"
#include "../render/VideoQuad.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

FileSource::FileSource(const char* path, ID3D11Device* d3dDevice)
    : m_path(path), m_d3dDevice(d3dDevice) {}

FileSource::~FileSource() { Close(); }

bool FileSource::Init() {
    Close();

    uint32_t ret = m_mediaSource.Open(m_path.c_str());
    if (ret != 0) {
        logger->error("FileSource: MediaSource::Open failed for {}", m_path);
        return false;
    }

    double frameRate = 0.0;
    ret = m_decoder.Init(m_mediaSource.GetFormatContext(), frameRate, m_d3dDevice);
    if (ret != 0) {
        logger->error("FileSource: VideoDecoder::Init failed");
        m_mediaSource.Close();
        return false;
    }

    m_frameRate = frameRate;
    m_frameDuration = (frameRate > 0.0) ? (1.0 / frameRate) : (1.0 / 30.0);
    return true;
}

bool FileSource::ReadFrame(VideoFrame& out, ID3D11DeviceContext* ctx, nv::VideoQuad* vq) {
    // Decode until we get a video frame or run out of packets
    for (;;) {
        AVPacket* packet = m_mediaSource.ReadPacket();
        if (!packet) {
            // Flush decoder
            av_frame_free(&m_frame);
            auto flushResult = m_decoder.Flush(0);
            if (flushResult.type == AVMEDIA_TYPE_VIDEO && flushResult.frame) {
                m_frame = flushResult.frame;
                break;
            }
            return false;  // EOF
        }

        auto decoded = m_decoder.SendAndReceive(packet);
        av_packet_free(&packet);

        if (decoded.type == AVMEDIA_TYPE_VIDEO) {
            av_frame_free(&m_frame);
            m_frame = decoded.frame;
            break;
        } else if (decoded.type == AVMEDIA_TYPE_AUDIO) {
            av_frame_free(&decoded.frame);
        }
    }

    if (!m_frame || !m_frame->data[0]) return false;

    // Copy decoded NV12 hardware frame into VideoQuad's shared texture
    HANDLE sharedHandle = vq->GetsharedHandle();
    TextureUpdater::Update(ctx, sharedHandle, m_frame, m_width, m_height, vq);

    out.texture = (ID3D11Texture2D*)m_frame->data[0];
    out.width = m_width;
    out.height = m_height;
    out.type = SourceType::File;
    return true;
}

void FileSource::Close() {
    av_frame_free(&m_frame);
    m_decoder.Close();
    m_mediaSource.Close();
}
