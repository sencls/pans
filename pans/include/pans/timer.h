#ifndef PANS_INCLUDE_PANS_TIMER
#define PANS_INCLUDE_PANS_TIMER

#include <chrono>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <optional>

#include <pans/export.h>
#include <pans/types.h>

namespace pans
{
    class TimerManager;
    class PANS_API Timer final : public std::enable_shared_from_this<Timer>
    {
    public:
        using ptr = std::shared_ptr<Timer>;
        using Callback = std::function<void()>;
        using Clock = std::chrono::steady_clock; // 不收单波脉冲影响
        using Duration = std::chrono::milliseconds;

        bool cancel();
        bool refresh();
        bool reset(Duration interval, bool from_now);
        bool reset(u64 milliseconds, bool from_now);

    private:
        friend class TimerManager;
        struct Comparator;
        Timer(Duration interval, Callback callback, bool recurring, TimerManager *manager);

    private:
        Duration m_interval{Duration::zero()};
        Clock::time_point m_next{};
        Callback m_callback;
        std::atomic<TimerManager *> m_manager{nullptr};
        bool m_recurring{false};
    };

    class PANS_API TimerManager
    {
    public:
        using Clock = Timer::Clock;
        using Duration = Timer::Duration;
        using Callback = Timer::Callback;
        TimerManager();
        virtual ~TimerManager();

        TimerManager(const TimerManager &) = delete;
        TimerManager(TimerManager &&) = delete;
        TimerManager &operator=(const TimerManager &) = delete;
        TimerManager &operator=(TimerManager &&) = delete;

        Timer::ptr addTimer(Duration interval, Callback callback, bool recurring = false);
        Timer::ptr addTimer(u64 miliseconds, Callback callback, bool recurring = false);

        Timer::ptr addConditionTimer(Duration interval, Callback callback, std::weak_ptr<void> condition, bool recurring = false);
        Timer::ptr addConditionTimer(u64 miliseconds, Callback callback, std::weak_ptr<void> condition, bool recurring = false);

        [[nodiscard]] std::optional<Duration> getNextTimeout() const;
        [[nodiscard]] u64 getNextTimer() const;
        void listExpiredCb(std::vector<Callback> &callbacks);

        [[nodiscard]] bool hasTimer() const;
        void cancelAllTimers();
        bool cancel(const Timer::ptr &timer);
        bool refresh(const Timer::ptr &timer);
        bool reset(const Timer::ptr &timer, Duration interval, bool from_now);
        bool reset(const Timer::ptr &timer, u64 milliseconds, bool from_now);

    protected:
        virtual void onTimerInsertedAtFront() = 0;

    private:
        friend class Timer;
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}

#endif