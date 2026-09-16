#ifndef PANS_INCLUDE_PANS_UTILS_SYSTEM_UTILS_H
#define PANS_INCLUDE_PANS_UTILS_SYSTEM_UTILS_H

#include <chrono>
#include <pans/export.h>
#include <pans/macros.h>

namespace pans
{
    [[nodiscard]] PANS_API std::chrono::steady_clock::duration GetElapsedTime() noexcept;
    [[nodiscard]] PANS_API u64 GetFiberId() noexcept;

} // namespace pans

#endif