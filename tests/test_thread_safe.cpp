#include<thread>
#include<iostream>
#include<vector>
#include<mutex>
#include<atomic>

std::atomic<int> money =0;
int nums=5;
int count =100000000;

std::mutex gs_mutex;

void add()
{
    
    for(int i=0;i<count;++i)
    {
        ++money;
    }
}

int main()
{
    std::vector<std::thread>threads;
    for(int i=0;i<nums;++i)
    {
        threads.emplace_back(
            []()->void{
                //std::lock_guard<std::mutex> lock(gs_mutex);
                //auto lock =std::lock_guard(gs_mutex);
                for(int i=0;i<count;++i)
                {
                    //auto lock =std::lock_guard(gs_mutex);
                    ++money;
                }}
        );
    }
    for(auto &t:threads)
    {
        t.join();
    }
    std::cout <<money <<std::endl;
    return 0;
}