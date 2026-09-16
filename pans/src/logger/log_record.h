#ifndef PANS_SRC_LOGGER_LOG_RECORD_H
#define PANS_SRC_LOGGER_LOG_RECORD_H

#include <chrono>
#include <cstdint>
#include <source_location>
#include <string_view>

#include <pans/logger/log_level.h>
#include <pans/macros.h>

namespace pans::detail
{
    struct LogRecordView
    {
        LogLevel::Level m_level = LogLevel::Level::LOG_LV_DEBUG;
        std::string_view m_loggerName = "";
        std::string_view m_message = "";
        std::chrono::system_clock::time_point m_timestamp; // 日志的时间戳
        std::chrono::system_clock::duration m_elapsed;     // 起服到已经经过的时间
        u64 m_threadId = 0;
        u64 m_fiberId = 0; // 协程id
        std::string_view m_threadName = "";
        std::string_view m_fileName = "";
        uint32_t m_line = 0;

        // std::string_view m_functionName = ""; // 函数名
        // std::source_location m_location;      // 表示文件位置，编译器可查,零运行时开销
    };

}

#endif
