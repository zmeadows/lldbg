#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int bar(int y)
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return y * 4321;
}

int foo(int x)
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return bar(x) * 1234;
}

int main(void)
{
    const std::vector<int> v = {1, 2, 3};

    const int x = foo(1);
    const int y = foo(-1);

    std::cout << "x = " << x << std::endl;
    std::cout << "y = " << y << std::endl;
    std::cout << "v[0] = " << v[0] << std::endl;

    return x + y;
};
