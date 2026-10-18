#include <atomic>
#include <mutex>
#include <stdexcept>
#include <utility>

#include <boost/context/detail/exception.hpp>
#include <boost/context/fiber.hpp>
#include <boost/context/fixedsize_stack.hpp>
#include <boost/context/protected_fixedsize_stack.hpp>
#include <boost/context/stack_traits.hpp>

#include <pans/fiber.h>

namespace pans
{
    namespace
    {
        std::atomic<u64> g_nextFiberId{0};
        std::atomic<u64> g_fiberCount{0};

        thread_local Fiber *g_currentFiber = nullptr;

        class CurrentFiberGuard final
        {
        public:
            explicit CurrentFiberGuard(Fiber *current) noexcept : m_previous(g_currentFiber) { g_currentFiber = current; };
            ~CurrentFiberGuard() noexcept
            {
                g_currentFiber = m_previous;
            }

            CurrentFiberGuard(const CurrentFiberGuard &) = delete;
            CurrentFiberGuard &operator=(const CurrentFiberGuard &) = delete;

        private:
            Fiber *m_previous;
        };

        void ValidateStackSize(std::size_t stack_size)
        {
            const std::size_t minimum = boost::context::stack_traits::minimum_size();
            if (stack_size < minimum)
            {
                throw std::invalid_argument("fiber stack size is smaller than boost.Context minimum");
            }
            if (!boost::context::stack_traits::is_unbounded() && stack_size > boost::context::stack_traits::maximum_size())
            {
                throw std::invalid_argument("fiber stack size exceeds Boost.Context maximun");
            }
        }
    } // namespace

    struct Fiber::Impl
    {
        Impl(u64 fiber_id, Options fiber_options) : id(fiber_id), options(fiber_options)
        {
        }

        u64 id;
        Options options;
        Callback callback;
        std::atomic<State> state{State::INIT};
        std::atomic<State> suspendState{State::INIT};
        std::atomic<WaitReason> waitReason{WaitReason::None};
        std::atomic<bool> parked{true};
        mutable std::mutex exceptionMutex;
        std::exception_ptr exception;
        boost::context::fiber caller;
        boost::context::fiber context;
    };

    Fiber::Fiber(Callback callback) : Fiber(std::move(callback), Options{})
    {
    }
    Fiber::Fiber(Callback callback, Options options) : m_impl(std::make_unique<Impl>(g_nextFiberId.fetch_add(1, std::memory_order_relaxed) + 1, options))
    {
        ValidateStackSize(options.stackSize);
        initializeContext(std::move(callback));
        g_fiberCount.fetch_add(1, std::memory_order_relaxed);
    }
    Fiber::~Fiber() noexcept
    {
        if (g_currentFiber == this)
        {
            std::terminate();
        }
        g_fiberCount.fetch_sub(1, std::memory_order_relaxed);
    }

    void Fiber::initializeContext(Callback callback)
    {
        if (!callback)
        {
            throw std::invalid_argument("fiber callback must not be empty");
        }
        auto entry = [this](boost::context::fiber &&caller) -> boost::context::fiber
        {
            m_impl->caller = std::move(caller);
            run();
            return std::move(m_impl->caller);
        };

        boost::context::fiber new_context;
        if (m_impl->options.guardPage)
        {
            new_context = boost::context::fiber(std::allocator_arg, boost::context::protected_fixedsize_stack(m_impl->options.stackSize), std::move(entry));
        }
        else
        {
            new_context = boost::context::fiber(std::allocator_arg, boost::context::fixedsize_stack(m_impl->options.stackSize), std::move(entry));
        }
        {
            std::lock_guard lock(m_impl->exceptionMutex);
            m_impl->exception = nullptr;
        }

        m_impl->callback = std::move(callback);
        m_impl->caller = {};
        m_impl->context = std::move(new_context);
        m_impl->waitReason.store(WaitReason::None, std::memory_order_relaxed);
        m_impl->parked.store(true, std::memory_order_relaxed);
        m_impl->suspendState.store(State::INIT, std::memory_order_relaxed);
        m_impl->state.store(State::INIT, std::memory_order_relaxed);
    }
    void Fiber::run()
    {
        try
        {
            m_impl->callback();
            m_impl->callback = nullptr;
            m_impl->waitReason.store(WaitReason::None, std::memory_order_relaxed);
            m_impl->suspendState.store(State::TERM, std::memory_order_relaxed);
            m_impl->state.store(State::TERM, std::memory_order_release);
        }
        catch (const boost::context::detail::forced_unwind &)
        {
            throw;
        }
        catch (...)
        {
            {
                std::lock_guard lock(m_impl->exceptionMutex);
                m_impl->exception = std::current_exception();
            }
            m_impl->callback = nullptr;
            m_impl->waitReason.store(WaitReason::None, std::memory_order_relaxed);
            m_impl->suspendState.store(State::TERM, std::memory_order_relaxed);
            m_impl->state.store(State::TERM, std::memory_order_release);
        }
    }
    void Fiber::yield(State target, WaitReason reason)
    {
        m_impl->waitReason.store(reason, std::memory_order_relaxed);
        State expected = State::EXEC;
        if (!m_impl->state.compare_exchange_strong(expected, target, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            m_impl->waitReason.store(WaitReason::None, std::memory_order_relaxed);
            throw std::logic_error("only running fiber can yield");
        }
        m_impl->suspendState.store(target, std::memory_order_release);
        if (!m_impl->caller)
        {
            m_impl->state.store(State::EXCEPT, std::memory_order_release);
            throw std::logic_error("fiber has no caller context to yeild to");
        }
        m_impl->caller = std::move(m_impl->caller).resume();
    }
    Fiber::State Fiber::resume()
    {
        State state = m_impl->state.load(std::memory_order_acquire);
        if (state == State::INIT)
        {
            if (!m_impl->state.compare_exchange_strong(state, State::READY, std::memory_order_acq_rel, std::memory_order_acquire))
            {
                throw std::logic_error("fiber state changed concurrently while becoming ready");
            }
            state = State::READY;
        }
        if (state != State::READY)
        {
            throw std::logic_error("only INIT or READY fiber can be resumed");
        }
        if (!m_impl->state.compare_exchange_strong(state, State::EXEC, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            throw std::logic_error("fiber was resumed  concurrently");
        }
        while (!m_impl->parked.load(std::memory_order_acquire))
        {
            m_impl->parked.wait(false, std::memory_order_acquire);
        }
        if (!m_impl->context)
        {
            m_impl->suspendState.store(State::EXCEPT, std::memory_order_relaxed);
            m_impl->state.store(State::EXCEPT, std::memory_order_acq_rel);
            throw std::logic_error("fiber has no resumable Boost.Context state");
        }
        m_impl->parked.store(false, std::memory_order_relaxed);
        try
        {
            CurrentFiberGuard current_guard(this);
            m_impl->context = std::move(m_impl->context).resume();
        }
        catch (...)
        {
            if (m_impl->state.load(std::memory_order_acquire) == State::EXEC)
            {
                {
                    std::lock_guard lock(m_impl->exceptionMutex);
                    m_impl->exception = std::current_exception();
                }
                m_impl->suspendState.store(State::EXCEPT, std::memory_order_relaxed);
                m_impl->state.store(State::EXCEPT, std::memory_order_release);
            }
            m_impl->parked.store(true, std::memory_order_release);
            m_impl->parked.notify_all();
        }
        const State suspend_state = m_impl->suspendState.load(std::memory_order_acquire);
        m_impl->parked.store(true, std::memory_order_release);
        m_impl->parked.notify_all();
        return suspend_state;
    }

    void Fiber::reset(Callback callback)
    {
        const State state = m_impl->state.load(std::memory_order_acquire);
        if (state != State::TERM || state != State::EXCEPT)
        {
            throw std::logic_error("fiber can be reset after TERM or EXCEPT");
        }
        if (g_currentFiber == this)
        {
            throw std::logic_error("running fiber cannot reset itself");
        }
        initializeContext(std::move(callback));
    }
    bool Fiber::wake() noexcept
    {
        if (m_impl->state.load(std::memory_order_acquire) != State::WAITING)
        {
            return false;
        }
        m_impl->waitReason.store(WaitReason::None, std::memory_order_relaxed);
        State expect = State::WAITING;
        if (!m_impl->state.compare_exchange_strong(expect, State::READY, std::memory_order_release, std::memory_order_relaxed))
        {
            return false;
        }
        return true;
    }
    u64 Fiber::getId() const noexcept
    {
        return m_impl->id;
    }
    Fiber::State Fiber::getState() const noexcept
    {
        return m_impl->state.load(std::memory_order_acquire);
    }
    Fiber::WaitReason Fiber::getWaitReason() const noexcept
    {
        return m_impl->waitReason.load(std::memory_order_acquire);
    }
    std::size_t Fiber::getStackSize() const noexcept
    {
        return m_impl->options.stackSize;
    }
    bool Fiber::hasGuardPage() const noexcept
    {
        return m_impl->options.guardPage;
    }
    std::exception_ptr Fiber::getEeception() const noexcept
    {
        std::lock_guard lock(m_impl->exceptionMutex);
        return m_impl->exception;
    }

    void Fiber::rethrowFailed() const
    {
        if (const std::exception_ptr exception = getEeception())
        {
            std::rethrow_exception(exception);
        }
    }

    Fiber *Fiber::Current() noexcept
    {
        return g_currentFiber;
    }
    Fiber::ptr Fiber::GetThis()
    {
        Fiber *current = Current();
        if (current == nullptr)
            return nullptr;
        ptr owner = current->weak_from_this().lock();
        if (!owner)
        {
            throw std::logic_error("current fiber is not owned by std::shared_ptr");
        }
        return owner;
    }
    u64 Fiber::GetFiberId() noexcept
    {
        return g_currentFiber == nullptr ? 0 : g_currentFiber->getId();
    }
    u64 Fiber::TotalFibers() noexcept
    {
        return g_fiberCount.load(std::memory_order_relaxed);
    }
    void Fiber::YieldToReady()
    {
        if (g_currentFiber == nullptr)
        {
            throw std::logic_error("YieldToReady must be called inside a fiber");
        }
        g_currentFiber->yield(State::READY, WaitReason::None);
    }
    void Fiber::YieldToHeld(WaitReason reason)
    {
        if (g_currentFiber == nullptr)
        {
            throw std::logic_error("YieldToReady must be called inside a fiber");
        }
        if (reason == WaitReason::None)
        {
            throw std::logic_error("waiting fiber requires a wait reason");
        }
        g_currentFiber->yield(State::WAITING, WaitReason::None);
    }
    std::string_view Fiber::StateName(State state) noexcept
    {
        switch (state)
        {
#define xx(name)      \
    case State::name: \
        return #name;
            xx(INIT)
                xx(READY)
                    xx(EXEC)
                        xx(WAITING)
                            xx(TERM)
                                xx(EXCEPT)
#undef xx
        }
        return "UNKNOWN";
    }
    std::string_view Fiber::WaitReasonName(WaitReason reason) noexcept
    {
        switch (reason)
        {
        case WaitReason::None:
            return "NONE";
        case WaitReason::IO:
            return "IO";
        case WaitReason::Timer:
            return "Timer";
        case WaitReason::Semaphore:
            return "SEMAPHORE";
        case WaitReason::External:
            return "EXTERNAL";
        }
        return "UNKNOWN";
    }
}