#ifndef PANS_INCLUDE_PANS_CONFIG_CONFIG_VAR_H
#define PANS_INCLUDE_PANS_CONFIG_CONFIG_VAR_H

#include <atomic>
#include <exception>
#include <memory>
#include <mutex>
#include <functional>
#include <string>

#include <pans/config/convert.h>
#include <pans/types.h>

namespace pans
{

    using ListenerId = u64;
    namespace detail
    {

        PANS_API void ReportConfigError(std::string_view operation, std::string_view name, std::string_view type_name, std::string_view reason);

        [[nodiscard]] PANS_API ListenerId NextListenerId() noexcept;

        [[nodiscard]] PANS_API std::string NormalizeConfigName(std::string name);

        [[nodiscard]] PANS_API bool IsValidConfigName(std::string_view name) noexcept;

    } // namespace detail

    class PANS_API ConfigVarBase
    {
    public:
        using ptr = std::shared_ptr<ConfigVarBase>;

        explicit ConfigVarBase(std::string name, std::string description = {});
        virtual ~ConfigVarBase();

        ConfigVarBase(const ConfigVarBase &) = delete;
        ConfigVarBase &operator=(const ConfigVarBase &) = delete;
        ConfigVarBase(ConfigVarBase &&) = delete;
        ConfigVarBase &operator=(ConfigVarBase &&) = delete;

        [[nodiscard]] const std::string &getName() const noexcept { return m_name; }
        [[nodiscard]] const std::string &getDescription() const noexcept { return m_description; }

        [[nodiscard]] virtual YAML::Node toNode() const = 0;

        virtual bool fromNode(const YAML::Node &node) = 0;
        [[nodiscard]] std::string toString() const;

        bool fromString(std::string_view value);

        [[nodiscard]] virtual std::string_view getTypeName() const noexcept = 0;

        virtual void removeListener(ListenerId id) = 0;
        virtual void clearListeners() = 0;
        [[nodiscard]] virtual std::size_t listenerCount() const = 0;

    protected:
        std::string m_name;
        std::string m_description;
    };

    template <typename T, typename Convert = ConfigConvert<T>>
    class ConfigVar final : public ConfigVarBase
    {
    public:
        using ptr = std::shared_ptr<ConfigVar>;
        using OnChange = std::function<void(const T &old_value, const T &new_value)>;

        ConfigVar(std::string name, T default_value, std::string description = {}) : ConfigVarBase(std::move(name), std::move(description)), m_value(std::make_shared<const T>(std::move(default_value))) {}
        ~ConfigVar() override
        {
            clearListeners();
        }

        [[nodiscard]] T getValue() const { return *m_value.load(std::memory_order_acquire); }
        [[nodiscard]] std::shared_ptr<const T> getSnapshot() const noexcept
        {
            return m_value.load(std::memory_order_acquire);
        }

        bool setValue(T value)
        {
            auto new_value = std::make_shared<const T>(std::move(value));
            std::shared_ptr<const T> old_value;
            {
                std::lock_guard<std::mutex> lock(m_writeMutex);
                old_value = m_value.load(std::memory_order_relaxed);
                if constexpr (std::equality_comparable<T>)
                {
                    if (*old_value == *new_value)
                    {
                        return false;
                    }
                }
                m_value.store(new_value, std::memory_order_release);
            }
            notify(*old_value, *new_value);
            return true;
        }

        [[nodiscard]] YAML::Node toNode() const override { return Convert::ToNode(*getSnapshot()); }

        bool fromNode(const YAML::Node &node) override
        {
            try
            {
                setValue(Convert::FromNode(node));
                return true;
            }
            catch (const std::exception &e)
            {
                detail::ReportConfigError("ConfigVar::fromNode", m_name, getTypeName(), e.what());
            }
            catch (...)
            {
                detail::ReportConfigError("ConfigVar::fromNode", m_name, getTypeName(), "未知异常");
            }
            return false;
        }

        [[nodiscard]] std::string_view getTypeName() const noexcept override { return TypeName<T>(); }

        ListenerId addLitener(OnChange callback)
        {
            if (!callback)
            {
                return 0;
            }
            const ListenerId id = detail::NextListenerId();
            auto shared_callback = std::make_shared<const OnChange>(std::move(callback));
            {
                std::lock_guard<std::mutex> lock(m_listenersMutex);
                m_listeners.emplac_back(id, std::move(shared_callback));
            }
            return id;
        }

        void removeListener(ListenerId id)
        {
            std::lock_guard<std::mutex> lock(m_listenersMutex);
            for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it)
            {
                if (it->m_id == id)
                {
                    m_listeners.erase(it);
                    return;
                }
            }
        }

        void clearListeners() override
        {
            std::lock_guard<std::mutex> lock(m_listenersMutex);
            m_listeners.clear();
        }

        [[nodiscard]] std::size_t listenerCount() const override
        {
            std::lock_guard<std::mutex> lock(m_listenersMutex);
            return m_listeners.size();
        }

    private:
        struct ListenerEntry
        {
            ListenerId m_id{0};
            std::shared_ptr<const OnChange> m_callback;
        };

        void notify(const T &old_value, const T &new_value) const
        {
            std::vector<ListenerEntry> snapshot;
            {
                std::lock_guard<std::mutex> lock(m_listenersMutex);
                snapshot = m_listeners;
            }
            for (const ListenerEntry &entry : snapshot)
            {
                try
                {
                    (*entry.m_callback)(old_value, new_value);
                }
                catch (const std::exception &e)
                {
                    detail::ReportConfigError("ConfigVar::listener", m_name, getTypeName(), e.what());
                }
                catch (...)
                {
                    detail::ReportConfigError("ConfigVar::listener", m_name, getTypeName(), "未知异常");
                }
            }
        }

    private:
        std::atomic<std::shared_ptr<const T>> m_value;
        std::mutex m_writeMutex;
        mutable std::mutex m_listenersMutex;
        std::vector<ListenerEntry> m_listeners;
    };
} // namespace pans

#endif
