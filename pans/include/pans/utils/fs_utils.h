#ifndef PANS_INCLUDE_PANS_UTILS_FS_UTILS_H
#define PANS_INCLUDE_PANS_UTILS_FS_UTILS_H

#include <string>
#include <string_view>
#include <vector>

#include <pans/export.h>

namespace pans
{
    class PANS_API FSUtils final
    {
    public:
        FSUtils() = delete;
        static void ListAllFile(std::vector<std::string> &files, std::string_view path, std::string_view suffix = {});
        [[nodiscard]] static bool MKdir(std::string_view dirname) noexcept;
        [[nodiscard]] static bool IsRunningPidfile(std::string_view pidfile) noexcept;
        [[nodiscard]] static bool Unlink(std::string_view filename, bool exist = false) noexcept;
    };
}

#endif