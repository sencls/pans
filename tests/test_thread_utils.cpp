#include <thread>
#include <string>
#include <iostream>

#if defined(__linux__)
#include <pthread.h>
#endif

#include "pans/utils/thread_utils.h"

int main()
{
    std::cout << "1. main thread id: " << pans::GetThreadId() << std::endl;
    std::cout << "2. main thread name: " << pans::GetThreadName() << std::endl;
    sleep(30); // top -H -p<pid>
    pans::SetThreadName("main_thread");
    std::cout << "3. main thread name: " << pans::GetThreadName() << std::endl;
    sleep(30);
    pans::SetThreadName("元神启动");
    std::cout
        << "4. main thread name: " << pans::GetThreadName() << std::endl;
    sleep(30);
    pans::SetThreadName("袁大头贵很值钱");
    std::cout
        << "5. main thread name: " << pans::GetThreadName() << std::endl;
    sleep(30);
    std::thread worker([]()
                       { std::cout << "6: son thread id: " << pans::GetThreadId() << std::endl; 
                    std::cout <<"7: son thread name: " <<pans::GetThreadName() <<std::endl; 
                sleep(30);
            pans::SetThreadName("son-thread-long-name");
            std::cout <<"8: son thread name: " <<pans::GetThreadName() <<std::endl;
            sleep(30);
            pans::SetThreadName("");
            std::cout <<"9: son thread name: " <<pans::GetThreadName() <<std::endl; });
    worker.join();
    return 0;
}
