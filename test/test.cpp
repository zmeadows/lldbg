#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

struct Thing {
    int x = 7;
    int y = 6;
    int z = 2;
};

int bar(int y) { return y * 4321; }

int foo(int x) { return bar(x) * 1234; }

int main(void)
{
    const std::vector<std::vector<int>> v = {{1, 2, 3, 4, 5, 6}, {7, 8, 9, 10}};

    const int x = foo(1);
    const int y = foo(-1);

    Thing thing;

    std::cout << "x = " << x << std::endl;
    std::cout << "y = " << y << std::endl;
    std::cout << "v[0][0] = " << v[0][0] << std::endl;
    std::cout << "thing.x = " << thing.x << std::endl;

    return x + y;
};
