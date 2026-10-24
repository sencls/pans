#include <atomic>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <pans/fiber.h>
#include <pans/macros.h>

namespace
{

    void TestFiber1()
    {
        std::vector<int> steps;
        pans::Fiber::ptr fiber = std::make_shared<pans::Fiber>([&]()
                                                               {
        ASSERT_RETNONE2(pans::Fiber::Current() == fiber.get(), "Current() did not return running fiber");
        ASSERT_RETNONE2(pans::Fiber::GetThis() == fiber, "GetThis() did not preserve shared ownership");
        ASSERT_RETNONE2(pans::Fiber::GetFiberId() == fiber->getId(), "running fiber id mismatch");
        steps.push_back(1);
        pans::Fiber::YieldToReady();
        steps.push_back(3); });
        ASSERT_RETNONE2(fiber->getState() == pans::Fiber::State::INIT, "new fiber must start in INIT");
        fiber->resume();
        ASSERT_RETNONE2(fiber->getState() == pans::Fiber::State::READY, "yielded fiber must be READY");
        steps.push_back(2);
        fiber->resume();
        ASSERT_RETNONE2(fiber->getState() == pans::Fiber::State::TERM, "completed fiber must be TERM");
        ASSERT_RETNONE2((steps == std::vector<int>{1, 2, 3}), "fiber execution order mismatch");
        ASSERT_RETNONE2(pans::Fiber::Current() == nullptr, "caller must not be reported as a fiber");
        ASSERT_RETNONE2(pans::Fiber::GetFiberId() == 0, "caller fiber id must be zero");
    }

    void TestFiber2()
    {
        int executions = 0;
        auto fiber = std::make_shared<pans::Fiber>([&]()
                                                   {
        ++executions;
        pans::Fiber::YieldToHold(pans::Fiber::WaitReason::IO);
        ++executions; });
        fiber->resume();
        ASSERT_RETNONE2(fiber->getState() == pans::Fiber::State::WAITING, "held fiber must be WAITING");
        ASSERT_RETNONE2(fiber->getWaitReason() == pans::Fiber::WaitReason::IO, "wait reason was not retained");
        bool rejected = false;
        try
        {
            fiber->resume();
        }
        catch (const std::logic_error &)
        {
            rejected = true;
        }
        ASSERT_RETNONE2(rejected, "WAITING fiber resumed without wake()");
        std::atomic<int> successful_wakes{0};
        std::vector<std::thread> wake_threads;
        for (int index = 0; index < 8; ++index)
        {
            wake_threads.emplace_back([&]()
                                      {
            if (fiber->wake())
            {
                successful_wakes.fetch_add(1, std::memory_order_relaxed);
            } });
        }
        for (std::thread &thread : wake_threads)
        {
            thread.join();
        }
        ASSERT_RETNONE2(successful_wakes.load(std::memory_order_relaxed) == 1, "never!absolutely never!");
        ASSERT_RETNONE2(fiber->getWaitReason() == pans::Fiber::WaitReason::None, "wake() must clear wait reason");
        fiber->resume();
        ASSERT_RETNONE2(fiber->getState() == pans::Fiber::State::TERM, "woken fiber did not terminate");
        ASSERT_RETNONE2(executions == 2, "woken fiber did not resume at suspension point");
    }

    void TestFiber3()
    {
        auto fiber = std::make_shared<pans::Fiber>([]()
                                                   { throw std::runtime_error("fiber failure"); });
        const std::uint64_t id = fiber->getId();
        fiber->resume();
        ASSERT_RETNONE2(fiber->getState() == pans::Fiber::State::EXCEPT, "throwing fiber must be EXCEPT");
        ASSERT_RETNONE2(fiber->getException() != nullptr, "fiber exception was not retained");
        bool rethrown = false;
        try
        {
            fiber->rethrowIfFailed();
        }
        catch (const std::runtime_error &exception)
        {
            rethrown = std::string(exception.what()) == "fiber failure";
        }
        ASSERT_RETNONE2(rethrown, "original fiber exception was not rethrown");

        bool reset_flag = false;
        fiber->reset([&]()
                     { reset_flag = true; });
        ASSERT_RETNONE2(fiber->getState() == pans::Fiber::State::INIT, "reset fiber must return to INIT");
        ASSERT_RETNONE2(fiber->getException() == nullptr, "reset fiber retained old exception");
        ASSERT_RETNONE2(fiber->getId() == id, "reset must preserve fiber identity");
        fiber->resume();
        ASSERT_RETNONE2(reset_flag && fiber->getState() == pans::Fiber::State::TERM, "reset task did not complete");
    }

    void TestFiber4()
    {
        bool invalid_size_rejected = false;
        try
        {
            pans::Fiber::Options invalid_options;
            invalid_options.stackSize = 1;
            auto invalid = std::make_shared<pans::Fiber>([]() {}, invalid_options);
            static_cast<void>(invalid);
        }
        catch (const std::invalid_argument &)
        {
            invalid_size_rejected = true;
        }
        ASSERT_RETNONE2(invalid_size_rejected, "invalid stack size was accepted");
        pans::Fiber::Options options;
        options.stackSize = pans::Fiber::DEFAULT_STACK_SIZE * 2;
        options.guardPage = false;
        auto fiber = std::make_shared<pans::Fiber>([]() {}, options);
        ASSERT_RETNONE2(fiber->getStackSize() == options.stackSize, "custom stack size was not retained");
        ASSERT_RETNONE2(!fiber->hasGuardPage(), "guard-page option was not retained");
        fiber->resume();
        ASSERT_RETNONE2(fiber->getState() == pans::Fiber::State::TERM, "custom-stack fiber did not terminate");
        ;
    }

}

int main()
{
    const std::uint64_t baseline = pans::Fiber::TotalFibers();
    TestFiber1();
    TestFiber2();
    TestFiber3();
    TestFiber4();

    ASSERT_RETVAL2(pans::Fiber::TotalFibers() == baseline, EXIT_FAILURE, "fiber lifetime counter leaked");
}
