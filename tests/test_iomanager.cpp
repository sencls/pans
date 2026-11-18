#include <future>
#include <fcntl.h>

#include <pans/iomanager.h>

namespace
{
    using namespace std::chrono_literals;

    void Check(bool condition, const char *message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    template <typename Future>
    void WaitReady(Future &future, const char *message)
    {
        if (future.wait_for(5s) != std::future_status::ready)
        {
            throw std::runtime_error(message);
        }
    }
} // namespace
