#include "Logger.h"

std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>("default");

void InitLogger(const char* logFilePath) {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    logger = spdlog::basic_logger_mt("file_logger", logFilePath);
    logger->set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::warn);
}
