#ifndef PANS_INCLUDE_PANS_UTILS_STRING_UTILS_H
#define PANS_INCLUDE_PANS_UTILS_STRING_UTILS_H

#include <cstring>
#include <cstdarg>
#include <string_view>

#include "pans/export.h"

namespace pans
{
    class PANS_API StringUtils final
    {
    public:
        StringUtils() = delete; // 不实例化，纯静态方法
        [[nodiscard]] static std::string WStringToString(std::wstring_view text) noexcept;
        [[nodiscard]] static std::wstring StringToWString(std::string_view text) noexcept;

        inline static constexpr std::string_view ASCII_WHITESPACE{" \t\n\r\f\v"};

        [[nodiscard]] static std::string Trim(const std::string &text, std::string_view delimiters = ASCII_WHITESPACE);
        [[nodiscard]] static std::string_view Trim(std::string_view text, std::string_view delimiters = ASCII_WHITESPACE) noexcept;
        [[nodiscard]] static std::string TrimLeft(const std::string &text, std::string_view delimiters = ASCII_WHITESPACE);
        [[nodiscard]] static std::string_view TrimLeft(std::string_view text, std::string_view delimiters = ASCII_WHITESPACE) noexcept;
        [[nodiscard]] static std::string TrimRight(const std::string &text, std::string_view delimiters = ASCII_WHITESPACE);
        [[nodiscard]] static std::string_view TrimRight(std::string_view text, std::string_view delimiters = ASCII_WHITESPACE) noexcept;
    };

}

#endif