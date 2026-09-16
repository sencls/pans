#ifndef PANS_INCLUDE_PANS_LOGGER_APPENDER_H
#define PANS_INCLUDE_PANS_LOGGER_APPENDER_H

#include <memory>
#include <string>

#include "pans/export.h"
#include "pans/logger/log_level.h"

namespace pans
{

    namespace detail
    {
        class AppenderAccess; // 私有实现，接口与实现分离实现，其作为内部的全权代理，友元代理
    }

    class PANS_API Appender final
    {
    public:
        class Impl;
        ~Appender() = default;
        Appender(const Appender &) = delete;
        Appender &operator=(const Appender &) = delete;
        Appender(Appender &&) = delete;
        Appender &operator=(Appender &&) = delete;

        void setLevel(LogLevel::Level level) noexcept;
        [[nodiscard]] LogLevel::Level getLevel() const noexcept;

        void flush(); // 将日志拿到脏页
        void sync();  // 将脏页拿到磁盘缓冲区

    private:
        explicit Appender(std::unique_ptr<Impl> impl) noexcept;
        std::unique_ptr<Impl> m_impl;
        friend class detail::AppenderAccess;
    };

    using AppenderPtr = std::shared_ptr<Appender>;

    [[nodiscard]] PANS_API AppenderPtr MakeStdoutAppender();
    [[nodiscard]] PANS_API AppenderPtr MakeFileAppender(std::string file_name);
    // to do 网络日志台j

} // namespace pans

#endif