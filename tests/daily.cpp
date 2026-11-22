#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

class Test
{
public:
    ~Test()
    {
        std::cout << "      ~Test()：对象被销毁\n";
    }

    void callback()
    {
        std::cout << "      callback() 开始\n";
        std::this_thread::sleep_for(200ms); // 模拟耗时回调，放大时间窗口
        std::cout << "      callback() 结束（此刻仍在访问 this）\n";
    }
};

void run(bool named)
{
    auto t = std::make_shared<Test>(); // 外部强引用（模拟会话对象）
    std::weak_ptr<Test> tt = t;        // 条件定时器拿到的弱引用
    Test *raw = t.get();               // 回调 lambda 里通常捕获的裸 this

    // 模拟另一个线程在回调执行期间，丢掉最后一个外部强引用
    std::thread releaser([&t]
                         {
        std::this_thread::sleep_for(100ms);   // 等 callback 刚开始就动手
        std::cout << "      [外部线程] t.reset()：释放最后一个外部强引用\n";
        t.reset(); });

    if (named)
    {
        // 方式二：命名的强引用 p，从 lock 成功一直活到整个 if 结束
        if (auto p = tt.lock())
        {
            std::cout << "      [方式二] lock 成功，p 为回调全程保活\n";
            p->callback();
        }
    }
    else
    {
        // 方式一：lock() 返回的临时对象在条件表达式结束（进入花括号前）就析构
        if (tt.lock())
        {
            std::cout << "      [方式一] lock 成功，但强引用已随条件表达式析构\n";
            tt.lock().get()->callback(); // 手上只剩裸指针，没有任何强引用保活
        }
    }

    releaser.join();
    std::cout << "      本轮结束\n";
}

int main()
{
    std::cout << "===== 方式一：if (tt.lock()) =====\n";
    run(false);

    std::cout << "\n===== 方式二：if (auto p = tt.lock()) =====\n";
    run(true);

    return 0;
}