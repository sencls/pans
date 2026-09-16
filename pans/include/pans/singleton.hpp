#ifndef PANS_INCLUDE_SINGLETON_HPP
#define PANS_INCLUDE_SINGLETON_HPP

#include <string>
#include <iostream>

namespace pans
{
    // class Singleton
    // {
    // public:
    //     Singleton(const Singleton &) = delete;
    //     Singleton(Singleton &&) = delete;
    //     Singleton &operator=(const Singleton &) = delete;
    //     Singleton &operator=(Singleton &&) = delete;

    //     static Singleton &Instance()
    //     {
    //         static Singleton instance; // 保证线程安全，guard_visible,对象的实际构造会延迟到实际使用的时候：懒汉式
    //         return instance;
    //     }

    //     void log(const std::string &msg)
    //     {
    //         std::cout << "[LOG]: " << msg << std::endl;
    //     }

    // private:
    //     Singleton() = default;
    //     ~Singleton() = default;
    // };

    // 变式1 全局访问点
    // template <typename T>
    // class Singleton
    // {
    // public:
    //     static inline T &Instance()
    //     {
    //         static T instance;
    //         return instance;
    //     }
    //     Singleton(const Singleton &) = delete;
    //     Singleton(Singleton &&) = delete;
    //     Singleton &operator=(const Singleton &) = delete;
    //     Singleton &operator=(Singleton &&) = delete;

    // private:
    //     Singleton() = delete;
    // };

    template <typename T>
    class Singleton
    {
    public:
        static T &Instance()
        {
            static T instance;
            return instance;
        }
        Singleton(const Singleton &) = delete;
        Singleton(Singleton &&) = delete;
        Singleton &operator=(const Singleton &) = delete;
        Singleton &operator=(Singleton &&) = delete;

    protected:
        Singleton() = default;
        ~Singleton() = default;
    };

}

#endif