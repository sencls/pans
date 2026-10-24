#ifndef PANS_INCLUDE_PANS_FIBER_H
#define PANS_INCLUDE_PANS_FIBER_H

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string_view>

#include <pans/export.h>
#include <pans/types.h>

namespace pans
{

    class PANS_API Fiber final : public std::enable_shared_from_this<Fiber>
    {
    public:
        using ptr = std::shared_ptr<Fiber>;
        using Callback = std::function<void()>;

        enum class State : u8
        {
            INIT,
            READY,
            EXEC,
            WAITING,
            TERM,
            EXCEPT,
        };

        enum class WaitReason : u8
        {
            None,
            IO,
            Timer,
            Semaphore,
            External,
        };

        static constexpr std::size_t DEFAULT_STACK_SIZE = 128U * 1024U;

        struct Options
        {
            std::size_t stackSize = DEFAULT_STACK_SIZE;
            bool guardPage = true;
        };

        explicit Fiber(Callback callback);
        Fiber(Callback callback, Options options);
        ~Fiber() noexcept;

        Fiber(const Fiber &) = delete;
        Fiber &operator=(const Fiber &) = delete;
        Fiber(Fiber &&) = delete;
        Fiber &operator=(Fiber &&) = delete;

        State resume();

        void reset(Callback callback);

        [[nodiscard]] bool wake() noexcept;
        [[nodiscard]] u64 getId() const noexcept;
        [[nodiscard]] State getState() const noexcept;
        [[nodiscard]] WaitReason getWaitReason() const noexcept;
        [[nodiscard]] std::size_t getStackSize() const noexcept;
        [[nodiscard]] bool hasGuardPage() const noexcept;
        [[nodiscard]] std::exception_ptr getException() const noexcept;

        void rethrowIfFailed() const;

        [[nodiscard]] static Fiber *Current() noexcept;
        [[nodiscard]] static ptr GetThis();
        [[nodiscard]] static u64 GetFiberId() noexcept;
        [[nodiscard]] static u64 TotalFibers() noexcept;

        static void YieldToReady();
        static void YieldToHold(WaitReason reason = WaitReason::External);

        [[nodiscard]] static std::string_view StateName(State state) noexcept;
        [[nodiscard]] static std::string_view WaitReasonName(WaitReason reason) noexcept;

    private:
        void initializeContext(Callback callback);
        void run();
        void yield(State target, WaitReason reason);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace pans

#endif // PANS_INCLUDE_PANS_FIBER_H
