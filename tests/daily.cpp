#include <iostream>
#include <algorithm>
#include <iomanip>
#include <vector>
#include <ranges>
#include <numeric>
#include <cmath>
#include <string>
#include <string_view>

class A
{
public:
    A() { std::cout << "默认构造函数\n"; }
    A(const A &) { std::cout << "拷贝构造函数\n"; }
    A(A &&) { std::cout << "移动构造\n"; }
    ~A() { std::cout << "析构\n"; }
};

A returnA()
{
    A a;
    return a;
}

int main()
{
    A a = returnA();
    return 0;
}
