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
    };

} // namespace pans

#endif