#ifndef PLATFORM_LOGGER_H
#define PLATFORM_LOGGER_H

#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

extern std::shared_ptr<spdlog::logger> logger;

void InitLogger(const char* logFilePath = "d3d_video.log");

#endif
