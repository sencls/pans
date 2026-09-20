#ifndef PANS_INCLUDE_LOGGER_LOG_LEVEL_H
#define PANS_INCLUDE_LOGGER_LOG_LEVEL_H

#include <string_view>

#include <pans/export.h>
#include <pans/types.h>

namespace pans
{

    class PANS_API LogLevel final
    {
    public:
        enum class Level : u8
        {
            LOG_LV_DEBUG = 1, // 记录调试细节，变量值，执行流程
            LOG_LV_INFO = 2,  // 记录正常运行信息
            LOG_LV_WARN = 3,  // 可能存在问题
            LOG_LV_ERROR = 4, // 操作失败，功能异常
            LOG_LV_FATAL = 5, // 严重故障，程序继续执行，可能存在较大问题
            LOG_LV_OFF = 6,   // 关闭日志
        };

        [[nodiscard]] static std::string_view ToString(Level level) noexcept;
        [[nodiscard]] static LogLevel::Level FromString(std::string_view value) noexcept;
    };

}

#endif