#include "VideoCtx.h"
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

extern std::shared_ptr<spdlog::logger> logger;

VideoCtx::VideoCtx()
{
	fmtCtx = nullptr;
}

VideoCtx::~VideoCtx()
{
}

MediaFrame VideoCtx::nextFrame()
{
	do {
		AVPacket* packet = av_packet_alloc();
		int ret = av_read_frame(fmtCtx, packet);
		if (ret == 0) {
			auto codecCtx = codecMap[packet->stream_index];
			ret = avcodec_send_packet(codecCtx, packet);
			if (ret == 0) {
				AVFrame* frame = av_frame_alloc();
				ret = avcodec_receive_frame(codecCtx, frame);
				if (ret == 0) {
					av_packet_unref(packet);
					return { codecCtx->codec_type, frame };
				}
				else if (ret == AVERROR(EAGAIN)) {
					av_frame_unref(frame);
				}
			}
		}
		else {
			return {AVMediaType::AVMEDIA_TYPE_UNKNOWN, nullptr};
		}

		av_packet_unref(packet);
	} while(1);

	return {AVMediaType::AVMEDIA_TYPE_UNKNOWN, nullptr};
}

uint32_t VideoCtx::Init(const char *filePath)
{
	int err = avformat_open_input(&fmtCtx, filePath, NULL, NULL);
	char x[256];
	av_make_error_string(x, 256, err);

	if (fmtCtx == nullptr)
	{
		logger->error("avformat_open_input failed: {}, file: {}", x, filePath);
		return -1;
	}
	avformat_find_stream_info(fmtCtx, NULL);
	return 0;
}

uint32_t VideoCtx::InitCodec(double &avg_frame_rate)
{
	AVCodecContext *vcodecCtx = nullptr;
	AVCodecContext *acodecCtx = nullptr;
	for (uint32_t i = 0; i < fmtCtx->nb_streams; i++)
	{
		auto theStream = fmtCtx->streams[i];
		const AVCodec *codec = avcodec_find_decoder(theStream->codecpar->codec_id);
		if (codec->type == AVMEDIA_TYPE_VIDEO)
		{
			avg_frame_rate = (double)theStream->avg_frame_rate.den / theStream->avg_frame_rate.num;
			vcodecCtx = avcodec_alloc_context3(codec);
			avcodec_parameters_to_context(vcodecCtx, theStream->codecpar);
			avcodec_open2(vcodecCtx, codec, NULL);
			codecMap[i] = vcodecCtx;

			AVBufferRef *hw_device_ctx = nullptr;
			av_hwdevice_ctx_create(&hw_device_ctx, AVHWDeviceType::AV_HWDEVICE_TYPE_D3D11VA, NULL, NULL, NULL);
			if (hw_device_ctx)
			{
				vcodecCtx->hw_device_ctx = hw_device_ctx;
			}
		}
		else if (codec->type == AVMEDIA_TYPE_AUDIO)
		{
			acodecCtx = avcodec_alloc_context3(codec);
			avcodec_parameters_to_context(acodecCtx, fmtCtx->streams[i]->codecpar);
			avcodec_open2(acodecCtx, codec, NULL);
			codecMap[i] = acodecCtx;
		}
	}

	return 0;
}

uint32_t VideoCtx::Deinit()
{
	for (auto it = codecMap.begin(); it != codecMap.end(); it++)
	{
		avcodec_free_context(&it->second);
	}

	codecMap.clear();

	avformat_close_input(&fmtCtx);
	return 0;
}

uint32_t VideoCtx::Reinit(const char *filePath, double &avg_frame_rate)
{
	Deinit();
	Init(filePath);
	if (fmtCtx == nullptr)
	{
		return -1;
	}
	InitCodec(avg_frame_rate);
	return 0;
}