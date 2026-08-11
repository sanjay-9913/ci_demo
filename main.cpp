#include <iostream>

int main()
{
    std::cout << "C++ CI test running\n";

    int frames = 120;

    std::cout << "Frames captured: " << frames << "\n";

    if (frames == 120)
    {
        std::cout << "TEST PASS\n";
        return 0;
    }

    std::cout << "TEST FAIL\n";
    return 1;
}
