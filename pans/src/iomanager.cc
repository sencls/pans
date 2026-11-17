#include <cstring>
#include <shared_mutex>
#include <unordered_set>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <pans/logger/log.h>
#include <pans/iomanager.h>
#include <pans/macros.h>

namespace pans
{
    namespace
    {
        const LoggerPtr g_logger = PNAS_LOG_NAME("system");
        constexpr int MAX_EPOLL_TIMEOUT_MS = 3000;
        constexpr std::size_t MAX_EPOLL_EVENTS = 256;
        constexpr u64 WAKE_TOKEN = MAX_U64;

        [[nodiscard]] std::unordered_set<IOManager *> &GetIOManagerRegistry()
        {
            static auto *manager = new std::unordered_set<IOManager *>;
            return *manager;
        }

        [[nodiscard]] std::mutex &GetIOManagerMutex()
        {
            static auto *s_mutex = new std::mutex;
            return *s_mutex;
        }

        void LogEpollError(const char *operation, int fd) noexcept
        {
            const int error = errno;
            LOG_ERROR << operation << " fd=" << fd << " failed:errno=" << error << " (" << std::strerror(error) << ")";
        }
    } // namespace

    struct IOManager::Impl
    {
        struct EventContext
        {
            using Callback = std::function<void()>;
            using Action = std::variant<std::monostate, Fiber::ptr, Callback>;
            Scheduler *scheduler{nullptr};
            Action action;

            [[nodiscard]] bool valid() const noexcept
            {
                if (scheduler == nullptr)
                    return false;
                if (const auto *fiber = std::get_if<Fiber::ptr>(&action))
                {
                    return static_cast<bool>(*fiber);
                }
                if (const auto *callback = std::get_if<Callback>(&action))
                {
                    return static_cast<bool>(*callback);
                }
                return false;
            }

            void reset() noexcept
            {
                scheduler = nullptr;
                action.emplace<std::monostate>();
            }
        };

        static_assert(std::is_nothrow_move_constructible_v<EventContext>);
        static_assert(std::is_nothrow_move_assignable_v<EventContext>);

        struct FdContext
        {
            explicit FdContext(int descriptor) : fd(descriptor) {}
            [[nodiscard]] EventContext &context(Event event)
            {
                if (event == Event::READ)
                {
                    return read;
                }
                if (event == Event::WRITE)
                {
                    return write;
                }
                throw std::invalid_argument("invalid IO event");
            }

            void advanceGeneration() noexcept
            {
                ++generation;
                if (generation == 0U)
                {
                    generation = 1U;
                }
            }

            std::mutex mutex;
            EventContext read;
            EventContext write;
            s32 fd{-1};
            u32 events{0U};
            u32 generation{1};
        };

        [[nodiscard]] FdContext *findContext(int fd) const
        {
            std::shared_lock lock(m_contextMutex);
            const auto it = m_contexts.find(fd);
            return it == m_contexts.end() ? nullptr : it->second.get();
        }

        [[nodiscard]] FdContext *getOrCreateContext(int fd)
        {
            {
                std::shared_lock lock(m_contextMutex);
                const auto it = m_contexts.find(fd);
                if (it != m_contexts.end())
                {
                    return it->second.get();
                }
            }
            std::unique_lock lock(m_contextMutex);
            auto [it, inserted] = m_contexts.try_emplace(fd);
            if (inserted)
            {
                it->second = std::make_unique<FdContext>(fd);
            }
            return it->second.get();
        }

        [[nodiscard]] bool updateInterest(FdContext &context, u32 new_events)
        {
            const int operation = context.events == 0 ? EPOLL_CTL_ADD : (new_events == 0U ? EPOLL_CTL_DEL : EPOLL_CTL_MOD);
            epoll_event event{};
            event.events = EPOLLET;
            if (new_events & static_cast<u32>(Event::READ))
            {
                event.events |= EPOLLIN;
            }
            if (new_events & static_cast<u32>(Event::WRITE))
            {
                event.events |= EPOLLOUT;
            }
            event.data.u64 = (static_cast<u64>(context.generation) << 32U) | static_cast<u32>(context.fd);
            if (::epoll_ctl(m_epollFd, operation, context.fd, &event))
            {
                if (new_events == 0U && (errno == EBADE || errno == ENOENT))
                {
                    context.advanceGeneration();
                    context.events = 0U;
                    return true;
                }
                LogEpollError("epoll_ctl", context.fd);
            }
            if (new_events == 0U)
            {
                context.advanceGeneration();
            }
            context.events = new_events;
            return true;
        }

        void dispatch(EventContext event) noexcept
        {
            if (!event.valid())
            {
                return;
            }
            try
            {
                if (auto *callback = std::get_if<EventContext::Callback>(&event.action))
                {
                    event.scheduler->addTask(std::move(*callback));
                }
                else if (auto *fiber = std::get_if<Fiber::ptr>(&event.action))
                {
                    if ((*fiber)->getState() == Fiber::State::WAITING)
                    {
                        (void)(*fiber)->wake();
                    }
                    event.scheduler->addTask(std::move(*fiber));
                }
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << "could not dispatch IO event: " << e.what();
            }
            catch (...)
            {
                LOG_ERROR << "could not dispatch IO event: unknown error";
            }
        }

        [[nodiscard]] EventContext takeEvent(FdContext &context, Event event)
        {
            EventContext result = std::move(context.context(event));
        }

        static_assert(static_cast<u32>(Event::READ) == static_cast<u32>(EPOLLIN));
        static_assert(static_cast<u32>(Event::WRITE) == static_cast<u32>(EPOLLOUT));
        void processReadyEvent(const epoll_event &event)
        {
            const u64 token = event.data.u64;
            if (token == WAKE_TOKEN)
            {
                u64 value = 0;
                while (::read(m_wakeFd, &value, sizeof(value) < 0 && errno == EINTR))
                {
                }
                return;
            }
            FdContext *context = findContext(static_cast<int>(static_cast<u32>(token)));
            if (context == nullptr)
            {
                return;
            }
            EventContext read_context, write_context;
            std::size_t triggered = 0;
            {
                std::lock_guard lock(context->mutex);
                if (context->generation != static_cast<u32>(token >> 22) || context->events == 0U)
                {
                    return;
                }
                u32 ready_bits = event.events;
                if (ready_bits & (EPOLLERR | EPOLLHUP))
                {
                    ready_bits |= EPOLLET | ((EPOLLIN | EPOLLOUT) & context->events);
                }
                u32 ready = ready_bits & (EPOLLIN | EPOLLOUT) & context->events;
                if (ready == 0U)
                {
                    return;
                }
                const u32 registered = context->events;
                const u32 remaining = registered & ~ready;
                u32 dispatch_events = ready;
                if (!updateInterest(*context, remaining))
                {
                    dispatch_events = registered;
                    context->events = 0U;
                    context->advanceGeneration();
                }
                if (dispatch_events & static_cast<u32>(Event::READ))
                {
                    read_context = takeEvent(*context, Event::READ);
                    ++triggered;
                }
                if (dispatch_events & static_cast<u32>(Event::WRITE))
                {
                    write_context = takeEvent(*context, Event::WRITE);
                    ++triggered;
                }
            }
            m_pendingEventCount.fetch_sub(triggered, std::memory_order_acq_rel);
            dispatch(std::move(read_context));
            dispatch(std::move(write_context));
        }

        std::atomic<bool> m_shuttingDown{false};
        std::mutex m_mutex;
        std::unordered_map<u64, std::function<void()>> m_waitCancellations;
        u64 m_nextWaitRegistration{0};
        int m_epollFd{-1};
        int m_wakeFd{-1};
        std::atomic<std::size_t> m_pendingEventCount{0};
        mutable std::shared_mutex m_contextMutex;
        std::unordered_map<int, std::unique_ptr<FdContext>> m_contexts;
    };

    IOManager::IOManager(std::size_t threads, bool use_caller, const std::string &name) : Scheduler(threads, use_caller, name), m_impl(std::make_unique<Impl>())
    {
        m_impl->m_epollFd = ::epoll_create1(EPOLL_CLOEXEC);
        if (m_impl->m_epollFd < 0)
        {
            throw std::system_error(errno, std::generic_category(), "epoll_create1");
        }
        m_impl->m_wakeFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC | EFD_SEMAPHORE);
        if (m_impl->m_wakeFd < 0)
        {
            const int error = errno;
            ::close(m_impl->m_epollFd);
            m_impl->m_epollFd = -1;
            throw std::system_error(error, std::generic_category(), "eventfd");
        }
        epoll_event event{};
        event.events = EPOLLIN; // IO多路复用，水平触发，边缘触发
        event.data.u64 = WAKE_TOKEN;
        if (::epoll_ctl(m_impl->m_epollFd, EPOLL_CTL_ADD, m_impl->m_wakeFd, &event) != 0)
        {
            const int error = errno;
            ::close(m_impl->m_wakeFd);
            ::close(m_impl->m_epollFd);
            m_impl->m_wakeFd = -1;
            m_impl->m_epollFd = -1;
            throw std::system_error(error, std::generic_category(), "epoll_ctl eventfd");
        }
        try
        {
            start();
            std::lock_guard registry_lock(GetIOManagerMutex());
            GetIOManagerRegistry().insert(this);
        }
        catch (...)
        {
            try
            {
                Scheduler::stop();
            }
            catch (...)
            {
            }
            ::close(m_impl->m_wakeFd);
            ::close(m_impl->m_epollFd);
            m_impl->m_wakeFd = -1;
            m_impl->m_epollFd = -1;
            throw;
        }
    }

    int IOManager::addEvent(int fd, Event event, std::function<void()> callback = {})
    {
        ASSERT_RETVAL(fd >= 0 && (event == Event::READ || event == Event::WRITE), -1);
        std::lock_guard lock(m_impl->m_mutex);
        if (m_impl->m_shuttingDown.load(std::memory_order_acquire))
        {
            errno = ECANCELED;
            return -1;
        }
        Impl::EventContext new_context;
        new_context.scheduler = Scheduler::GetThis();
        if (callback)
        {
            new_context.action.emplace<Impl::EventContext::Callback>(std::move(callback));
            if (new_context.scheduler == nullptr)
            {
                new_context.scheduler = this;
            }
        }
        else
        {
            Fiber::ptr fiber = Fiber::GetThis();
            ASSERT_RETVAL(new_context.scheduler && fiber && fiber->getState() == Fiber::State::EXEC, -1);
            new_context.action.emplace<Fiber::ptr>(std::move(fiber));
        }
        Impl::FdContext *context = m_impl->getOrCreateContext(fd);
        std::lock_guard ctx_lock(context->mutex);
        if (context->events & static_cast<u32>(event))
        {
            LOG_ERROR << "addEvent fd=" << fd << " event=" << static_cast<u32>(context->events);
            ASSERT_RETVAL(false, -1);
        }
        const u32 new_evnets = context->events | static_cast<u32>(event);
        if (!m_impl->updateInterest(*context, new_evnets))
        {
            ASSERT_RETVAL(false, -2);
        }
        context->context(event) = std::move(new_context);
        m_impl->m_pendingEventCount.fetch_add(1, std::memory_order_acq_rel);
        return 0;
    }
    bool IOManager::delEvent(int fd, Event event)
    {
        Impl::FdContext *context = m_impl->findContext(fd);
        if (context == nullptr)
        {
            return false;
        }
        [[maybe_unused]] Impl::EventContext discarded;
        {
            std::lock_guard lock(context->mutex);
            const u32 event_mask = static_cast<u32>(event);
            if (!(context->events & event_mask))
            {
                return false;
            }
            if (!m_impl->updateInterest(*context, context->events & ~event_mask))
            {
                return false;
            }
            discarded = m_impl->takeEvent(*context, event);
        }
        m_impl->m_pendingEventCount.fetch_sub(1, std::memory_order_acq_rel);
    }

    bool IOManager::cancelEvent(int fd, Event event)
    {
        Impl::FdContext *context = m_impl->findContext(fd);
        if (context == nullptr)
        {
            return false;
        }
        Impl::EventContext canceled;
        {
            std::lock_guard lock(context->mutex);
            const u32 event_mask = static_cast<u32>(event);
            if (!(context->events & event_mask))
            {
                return false;
            }
            if (!m_impl->updateInterest(*context, context->events & ~event_mask))
            {
                return false;
            }
            canceled = m_impl->takeEvent(*context, event);
        }
        m_impl->m_pendingEventCount.fetch_sub(1, std::memory_order_acq_rel);
        m_impl->dispatch(std::move(canceled));
        return true;
    }

    bool IOManager::cancelAll(int fd)
    {
        Impl::FdContext *context = m_impl->findContext(fd);
        if (context == nullptr)
        {
            return false;
        }
        Impl::EventContext read_context;
        Impl::EventContext write_context;
        std::size_t canceled = 0;
        {
            std::lock_guard lock(context->mutex);
            if (context->events == 0U)
            {
                return false;
            }
            const u32 old_events = context->events;
            if (!m_impl->updateInterest(*context, 0U))
            {
                return false;
            }
            if (old_events & static_cast<u32>(Event::WRITE))
            {
                write_context = m_impl->takeEvent(*context, Event::WRITE);
                ++canceled;
            }
        }
        m_impl->m_pendingEventCount.fetch_sub(canceled, std::memory_order_acq_rel);
        m_impl->dispatch(std::move(read_context));
        m_impl->dispatch(std::move(write_context));
        return true;
    }

    bool IOManager::CancelFdAcrossManagers(int fd)
    {
        bool canceled = false;
        std::lock_guard registry_lock(GetIOManagerMutex());
        for (IOManager *manager : GetIOManagerRegistry())
        {
            canceled = manager->cancelAll(fd) || canceled;
        }
        return canceled;
    }
    bool IOManager::CancelEventAcrossManagers(int fd, Event event)
    {
        bool canceled = false;
        std::lock_guard lock(GetIOManagerMutex());
        for (auto *manager : GetIOManagerRegistry())
        {
            canceled = manager->cancelEvent(fd, event) | canceled;
        }
        return canceled;
    }

    IOManager *IOManager::GetThis() noexcept
    {
        return dynamic_cast<IOManager *>(Scheduler::GetThis());
    }

    u64 IOManager::registerCancellationAction(std::function<void()> action)
    {
        ASSERT_RETVAL2(action, INVALID64, "wait cancellation action must not be empty");
        std::lock_guard lock(m_impl->m_mutex);
        if (m_impl->m_shuttingDown.load(std::memory_order_acquire))
        {
            return 0;
        }
        const u64 key = ++m_impl->m_nextWaitRegistration;
        m_impl->m_waitCancellations.emplace(key, std::move(action));
        return key;
    }
    void IOManager::unregisterCancellationAction(u64 key) noexcept
    {
        [[maybe_unused]] std::function<void()> discarded;
        {
            std::lock_guard lock(m_impl->m_mutex);
            const auto it = m_impl->m_waitCancellations.find(key);
            if (it == m_impl->m_waitCancellations.end())
            {
                return;
            }
            discarded = std::move(it->second);
            m_impl->m_waitCancellations.erase(it);
        }
    }
    bool IOManager::isShuttingDown() const noexcept
    {
        return m_impl->m_shuttingDown.load(std::memory_order_acquire);
    }

    void IOManager::tickle()
    {
        constexpr u64 ONE = 1;
        ssize_t result = 0;
        do
        {
            result = ::write(m_impl->m_wakeFd, &ONE, sizeof(ONE));
        } while (result < 0 &&errno = EINTR);
        if (result < 0 && errno != EAGAIN)
        {
            LogEpollError("eventfd write", m_impl->m_wakeFd);
        }
    }
    bool IOManager::canStop(u64 &timeout)
    {
        timeout = getNextTimer();
        return timeout == INVALID64 && m_impl->m_pendingEventCount.load(std::memory_order_acquire) == 0 && Scheduler::canStop();
    }
    bool IOManager::canStop()
    {
        u64 timeout = INVALID64;
        return canStop(timeout);
    }
    void IOManager::idle()
    {
        std::array<epoll_event, MAX_EPOLL_EVENTS> events{};
        while (true)
        {
            int ready_count = 0;
            do
            {
                u64 next_timeout = INVALID64;
                if (canStop(next_timeout))
                {
                    return;
                }
                const int timeout = next_timeout == INVALID64 ? MAX_EPOLL_TIMEOUT_MS : static_cast<int>(std::min(next_timeout, MAX_EPOLL_TIMEOUT_MS));
                ready_count = ::epoll_wait(m_impl->m_epollFd, events.data(), static_cast<int>(events.size()), timeout);
            } while (ready_count < 0 && errno == EINTR);

            std::vector<TimerManager::Callback> callbacks;
            listExpiredCb(callbacks);
            if (!callbacks.empty())
            {
                addTask(std::make_move_iterator(callbacks.begin()), std::make_move_iterator(callbacks.end()));
            }
            for (int index = 0; index < ready_count; ++index)
            {
                m_impl->processReadyEvent(events[static_cast<std::size_t>(index)]);
            }
            Fiber::YieldToHold(Fiber::WaitReason::External);
        }
    }
    void IOManager::onTimerInsertedAtFront()
    {
        tickle();
    }

} // namespace pans
