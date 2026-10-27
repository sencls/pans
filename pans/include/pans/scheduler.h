#ifndef PANS_INCLUDE_PANS_SCHEDULER_H
#define PANS_INCLUDE_PANS_SCHEDULER_H

#include <atomic>
#include <condition_variable>
#include <concepts>
#include <variant>
#include <functional>
#include <iosfwd>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <pans/export.h>
#include <pans/fiber.h>
#include <pans/types.h>

namespace pans
{
    class PANS_API Scheduler
    {
    public:
        using ptr = std::shared_ptr<Scheduler>;
        using ThreadId = u64;

        static constexpr ThreadId ANY_THREAD = 0;

        explicit Scheduler(std::size_t threads = 1, bool use_caller = true, const std::string &name = "");

        virtual ~Scheduler();
        Scheduler(const Scheduler &) = delete;
        Scheduler &operator=(const Scheduler &) = delete;
        Scheduler(Scheduler &&) = delete;
        Scheduler &operator=(Scheduler &&) = delete;

        [[nodiscard]] const std::string &getName() const noexcept { return m_name; }
        [[nodiscard]] static Scheduler *GetThis() noexcept;
        [[nodiscard]] static Fiber *GetMainFiber() noexcept;

        void start();

        void stop();

        template <typename FiberOrCallback>
        void addTask(FiberOrCallback &&task, ThreadId thread_id = ANY_THREAD)
        {
            bool need_tickle = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                need_tickle = schedulerNoLock(std::forward<FiberOrCallback>(task), thread_id);
            }
            if (need_tickle)
            {
                tickle();
            }
        }

        template <typename InputIterator>
        void addTask(InputIterator begin, InputIterator end)
        {
            bool need_tickle = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                while (begin != end)
                {
                    need_tickle = SchedulerNoLock(*begin, ANY_THREAD) || need_tickle;
                    ++begin;
                }
            }
            if (need_tickle)
            {
                tickle();
            }
        }

        void switchTo(ThreadId thread_id = ANY_THREAD);

        std::ostream &dump(std::ostream &output) const;

    private:
        virtual void tickle();

        void run();

        [[nodiscard]] virtual bool canStop();

        virtual void idle();

        void setThis() noexcept;

        [[nodiscard]] bool hasIdleThreads() const noexcept
        {
            return m_idleThreadCount.load(std::memory_order_acquire) != 0;
        }

    private:
        using Callback = std::function<void()>;
        using Work = std::variant<std::monostate, Fiber::ptr, Callback>;

        struct Task
        {
            Work work;
            ThreadId threadId{ANY_THREAD};
            Task() = default;
            Task(Fiber::ptr fiber, ThreadId thread_id) : work(std::move(fiber)), threadId(thread_id) {}

            Task(Callback callback, ThreadId thread_id) : work(std::move(callback)), threadId(thread_id) {}

            [[nodiscard]] Fiber::ptr *fiber() noexcept { return std::get_if<Fiber::ptr>(&work); }
            [[nodiscard]] Callback *Callback() noexcept { return std::get_if<Callback>(&work); }

            [[nodiscard]] bool valid() const noexcept
            {
                if (const auto *value = std::get_if<Fiber::ptr>(&work))
                {
                    return static_cast<bool>(*value);
                }
                if (const auto *value = std::get_if<Callback>(&work))
                {
                    return static_cast<bool>(*value);
                }
            }
        };

        struct FetchResult
        {
            Task task;
            bool tickleOthers{false};
            bool retryPending{false};
        };

        [[nodiscard]] static Task MakeTask(Fiber::ptr fiber, ThreadId thread_id)
        {
            return Task(std::move(fiber), thread_id);
        }

        template <typename Callable>
            requires std::invocable < std::decay_t<Callable> &&std::constructible_from<Callback, Callable> [[nodiscard]] static Task MakeTask(Callable &&callback, ThreadId thread_id){
                                          return Task(Callback(std::forward<Callable>(callback)), thread_id);

    }

    template <typename FiberOrCallback>
    bool scheduleNoLock(FiberOrCallback &&task, ThreadId thread_id)
    {
        Task t = MakeTask(std::forward<FiberOrCallback>(task), thread_id);
        if (!t.valid())
        {
            return false;
        }
        const bool need_tickle = m_tasks.empty() || hasIdleThreads();
        m_tasks.emplace_back(std::move(t));
        return need_tickle;
    }

    [[nodiscard]] FetchResult fetchTask(ThreadId current_thread);

    [[nodiscard]] bool hasRunnableTaskNoLock(ThreadId current_thread) const;

    [[nodiscard]] bool canStopNoLock() const noexcept;

    void taskFinished() noexcept;

    void reportFiberFailure(const Fiber::ptr &fiber) const noexcept;

private:
    std::string m_name;
    std::condition_variable m_condition;
    std::vector<std::thread> m_threads;
    std::list<Task> m_tasks;
    Fiber::ptr m_rootFiber;
    mutable std::mutex m_mutex;
    bool m_useCaller{false};
    bool m_started{false};
    std::size_t m_registerWorkers{0};

protected:
    std::vector<ThreadId> m_threadIds;
    std::size_t m_threadCount{0};
    std::atomic<std::size_t> m_activeThreadCount{0};
    std::atomic<std::size_t> m_idleThreadCount{0};
    std::atomic<bool> m_stopping{false};
    std::atomic<bool> m_autoStop{false};
    ThreadId m_rootThreadId{ANY_THREAD};
};

class PANS_API SchedulerSwitcher final
{
public:
    explicit SchedulerSwitcher(Scheduler *target = nullptr);
    ~SchedulerSwitcher() noexcept;

    SchedulerSwitcher(const SchedulerSwitcher &) = delete;
    SchedulerSwitcher(SchedulerSwitcher &&) = delete;
    SchedulerSwitcher &operator=(const SchedulerSwitcher &) = delete;
    SchedulerSwitcher &operator=(SchedulerSwitcher &&) = delete;

private:
    Scheduler *m_caller = nullptr;
};
}

#endif