#include <iostream>
#include <algorithm>
#include <iomanip>
#include <vector>
#include <ranges>
#include <numeric>
#include <cmath>

auto Qvalue(const int &total)
{
    auto cal = [](auto &sum, auto &cnt) -> double
    { return (sum * sum) / ((cnt + 1) * cnt); };

    auto ration = [](auto &total, auto &sum, auto &x) -> double
    { return total * (x / (sum)); };
    double sum;
    std::vector<double> nums{192, 223, 421, 100, 209, 211, 969, 316, 175, 629}; //, 209, 211, 969, 316, 175, 629};
    std::vector<double> rations;
    // std::cin >> total;
    sum = std::accumulate(nums.begin(), nums.end(), 0);
    // std::cout << "标准分类：-----------------------------\n";
    for (size_t i = 0; i < nums.size(); ++i)
    {
        double temp = ration(total, sum, nums[i]);
        // std::cout << "第" << i + 1 << "方: " << temp << std::endl;
        rations.emplace_back(temp);
    }

    // std::cout << "Q值:-----------------------------------\n";
    std::vector<int> cnts(static_cast<int>(nums.size()), 1);
    // std::cout << cnts.size();
    for (int i = static_cast<int>(nums.size()); i < total; ++i)
    {
        double maxn = 0;
        int pos = 0;
        for (int k = 0; k < nums.size(); ++k)
        {
            double temp = cal(nums[k], cnts[k]);
            if (temp == maxn)
            {
                std::cout << "过程中出现相同极大值\n";
            }
            if (temp > maxn)
            {
                maxn = temp;
                pos = k;
            }
        }
        cnts[pos]++;
    }
    bool flag = true;
    std::cout << "最后结果如下：\n";
    for (int i = 0; i < nums.size(); ++i)
    {
        std::cout << i + 1 << " " << rations[i] << " " << cnts[i];
        if (cnts[i] != std::floor(rations[i]) && cnts[i] != std::ceil(rations[i]))
        {
            std::cout << "     该席位分配出现问题";
            flag = false;
        }
        std::cout << std::endl;
    }
    return flag;
}

int main()
{
    for (int i = 1662; i <= 3445; ++i)
    {
        std::cout << "总席位为：" << i << std::endl;
        if (!Qvalue(i))
            return 0;
    }
    return 0;
}

// 32 //423 //553
// 32 //558 //1447 //1611 //1725