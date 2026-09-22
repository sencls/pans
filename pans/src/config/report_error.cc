#include <stdexcept>
#include <string>
#include <string_view>

#include <pans/config/convert.h>

namespace pans::detail
{
    void ThrowConvertError(std::string_view type_name, std::string_view reason, std::string_view value)
    {
        std::string message;
        message.reserve(type_name.size() + reason.size() + value.size() + 32);
        message.append("ConfigConvert<").append(type_name).append(">: ").append(reason);
        if (!value.empty())
        {
            message.append(", value=").append(value);
        }
        throw std::runtime_error(message);
    }
    bool ParseBool(std::string_view text, bool &out) noexcept;
    const std::string &ScalarText(const YAML::Node &node, std::string_view type_name);

}