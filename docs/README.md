参考 sylar 服务器框架的架构思想，从零独立设计重写的 Linux C++ 服务器基础库。采用 include/src 分离、pimpl + 命名空间 + PANS_API 导出宏的正规库结构，已实现 配置 / 日志 / 协程 / 同步原语 四大模块，协程调度器与 IO 模块开发中。

用户态协程：基于 boost::context::fiber + protected_fixedsize_stack 实现非对称协程，支持保护页栈防越界、std::exception_ptr 跨协程异常传递（rethrowFailed()），并设计 WaitReason{IO, Timer, Semaphore, External} 等待原因状态机，为后续协程调度器铺路（对比原版 ucontext 方案：更轻量、跨平台）
线程安全配置系统（原创设计）：模板类 ConfigVar<T, Convert> 配合 ConfigConvert trait 定制类型转换；采用 atomic<shared_ptr<const T>> 不可变快照——读路径完全无锁、写时复制，if constexpr + C++20 concept 做变更去重，支持 OnChange(old, new) 监听器与集中式错误上报；底层 yaml-cpp 加载
高性能日志模块：LogLine 一次捕获时间戳/耗时/线程 ID/协程 ID/线程名；实现 SSO 思想的 InlineBuffer（栈上内联缓冲、溢出才落堆）并通过继承 std::streambuf 兼容 C++ ostream 生态；编译期宏仅传递文件名而非全路径，降低二进制体积与运行期开销
平台自适应同步原语：自旋锁在 Linux/glibc 下走 pthread_spinlock_t，其余平台以 atomic_flag 实现 TTAS 自旋 + test() 探测，并用 _mm_pause（x86）/ __yield（ARM）降低总线争用
工程化与质量：CMake ≥ 3.25 现代 target 化管理，完备 install/export 配置（下游可直接 find_package(pans) 消费）；GNU/Clang 下开启 -Werror；编写 13 个 CTest 测试可执行程序覆盖 config/logger/mutex/singleton/thread 安全性等核心模块
开发中：Scheduler 协程调度器、基于 epoll 的 IO 协程切换、定时器模块（kcp/zlib/abseil 等依赖已完成工程集成预留）。