
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <vector>
#include <ranges>
#include <numeric>
#include <cmath>
#include <string>
#include <string_view>

int main()
{
    std::string s = "Scut2024";
    for (auto &t : s)
    {
        t = static_cast<ushort>(t) ^ static_cast<ushort>(s.size());
    }
    std::cout << s;
    return 0;
}
