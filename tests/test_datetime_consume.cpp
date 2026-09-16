#include <iostream>
#include <chrono>
#include <ctime>

void test_time(int times)
{
    const std::string &format = "%Y-%m-%d %H:%M:%S";
    static thread_local time_t last_second = 0;
    static thread_local char cached_date_time[20] = {'\0'};
    auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < times; ++i)
    {
    }
    auto t2 = std::chrono::steady_clock::now();
    for (int i = 0; i < times; ++i)
    {
        struct tm tm;
        time_t now = time(0);
        localtime_r(&now, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), format.c_str(), &tm);
    }
    auto t3 = std::chrono::steady_clock::now();
    for (int i = 0; i < times; ++i)
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        time_t current_second = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

        /**
         * localtime_r和strftime的性能消耗是chrono::system_clock的10~15倍
         * 简单的优化是，当秒数发生变化的时候，再去执行转换，如果秒数没变，直接使用上一次的字符串
         */
        if (current_second != last_second)
        {
            struct tm buf;
            localtime_r(&current_second, &buf);
            strftime(cached_date_time, sizeof(cached_date_time), "%Y-%m-%d %H:%M:%S", &buf);
            last_second = current_second;
        }
    }
    auto t4 = std::chrono::steady_clock::now();
    auto base = t2 - t1;
    auto diff1 = t3 - t2 - base;
    auto diff2 = t4 - t3 - base;

    // 5. 转换为具体数值
    auto base_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(base).count();
    auto diff1_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(diff1).count();
    auto diff2_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(diff2).count();

    std::cout << "基础耗时: " << base_ns << " ns" << std::endl;
    std::cout << "原耗时: " << diff1_ns << " ns" << std::endl;
    std::cout << "现耗时: " << diff2_ns << " ns" << std::endl;
    std::cout << "优化倍数: " << (double)diff1_ns / diff2_ns << std::endl;
}

int main(int argc, char **argv)
{
    int times = 0;
    if (argc < 2)
    {
        times = 1000000;
    }
    else
    {
        times = std::stoull(argv[1]);
    }
    test_time(times);
    return 0;
}