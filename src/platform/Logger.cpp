#include "Logger.h"

std::shared_ptr<spdlog::logger> logger;

void InitLogger(const char* logFilePath) {
    if (logger) return;
    logger = spdlog::basic_logger_mt("file_logger", logFilePath);
    logger->set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::warn);
}
