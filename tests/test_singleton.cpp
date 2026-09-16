#include <iostream>
#include <thread>
#include <vector>

#include <pans/singleton.hpp>

// int main()
// {
//     pans::Singleton::Instance().log("Hello Singleton.");
//     return 0;
//}

// class A
// {
// public:
//     void log(const std::string &msg)
//     {
//         std::cout << "[LOG]: " << msg << std::endl;
//     }
//     A(const A &) = delete;
//     A(A &&) = delete;
//     A &operator=(const A &) = delete;
//     A &operator=(A &&) = delete;

// private:
//     A() = default;
//     ~A() = default;

//     friend class pans::Singleton<A>;
// };

// using SA = pans::Singleton<A>;

class A : public pans::Singleton<A>
{
    friend class pans::Singleton<A>;

public:
    void log(const std::string &msg)
    {
        std::cout << "[LOG]: " << msg << std::endl;
        std::cout << "data: " << m_data << std::endl;
    }

private:
    A()
    {
        std::puts("A: A() -- only once");
        m_data++;
    }
    ~A()
    {
        std::puts("A::~A() --only once");
    }

private:
    int m_data{0};
};
int main()
{
    std::vector<std::thread> threads;
    threads.reserve(10);
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([]()
                             { A::Instance().log("Hello Singleton3."); });
    }
    for (int i = 0; i < 10; ++i)
    {
        threads[i].join();
    }
    return 0;
}