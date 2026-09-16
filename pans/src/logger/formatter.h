#ifndef PANS_SRC_LOGGER_FORMATTER_H
#define PANS_SRC_LOGGER_FORMATTER_H

#include <memory>
#include <string>
#include <vector>

#include "logger/buffer_config.h"
#include "logger/buffer.h"
#include "log_record.h"

namespace pans::detail
{
    using FormattedRecordBuffer = InlineBuffer<FORMATTED_RECORD_INLINE_CAPACITY>;

    class Formatter final
    {
    public:
        class FormatItem
        {
        public:
            virtual ~FormatItem() = default;
            virtual void format(const LogRecordView &record, FormattedRecordBuffer &output) const = 0;

            // private:
            //     std::string m_value;
        };

        explicit Formatter(std::string_view pattern);
        void format(const LogRecordView &record, FormattedRecordBuffer &output) const;
        [[nodiscard]] const std::string &getPattern() const noexcept { return m_pattern; }

    private:
        int parse();
        void addLiteral(std::string &literal);

    private:
        std::string m_pattern = "";
        std::vector<std::unique_ptr<FormatItem>> m_items;
    };
}
#endif