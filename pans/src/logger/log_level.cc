#include <pans/logger/log_level.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <array>

namespace pans
{

    std::string_view LogLevel::ToString(Level level) noexcept
    {
        switch (level)
        {
#define xx(name)               \
    case Level::LOG_LV_##name: \
        return #name;
            xx(DEBUG)
                xx(INFO)
                    xx(WARN)
                        xx(ERROR)
                            xx(FATAL)
                                xx(OFF)
#undef xx
        }
        return "UNKNOWN";
    }

    LogLevel::Level LogLevel::FromString(std::string_view value) noexcept
    {
        std::array<char, 6> normalized{};
        std::transform(value.begin(), value.end(), normalized.begin(), [](unsigned char ch)
                       { return static_cast<char>(std::toupper(ch)); });
        const std::string_view upper_value(normalized.data(), value.size());
#define xx(name)              \
    if (upper_value == #name) \
        return Level::LOG_LV_##name;
        xx(DEBUG)
            xx(INFO)
                xx(WARN)
                    xx(ERROR)
                        xx(FATAL)
                            xx(OFF)
#undef xx
                                return Level::LOG_LV_OFF;
    }
}