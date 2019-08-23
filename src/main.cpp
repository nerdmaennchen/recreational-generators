#include <iostream>

#include "Generator.h"

int main() {
    auto gen = [](auto& t) {
        struct Blubb {
            ~Blubb() {
                std::cout << "destructor called" << std::endl;
            }
        };
        Blubb blubb{};
        for (int i{0}; i < 10; ++i) {
            t.yield(i);
        }
    };
    for (auto i : Generator<int>{gen}) {
        std::cout << i << "\n";
        break;
    }
    for (auto i : Generator<int>{gen}) {
        std::cout << i << "\n";
    }
    std::cout << "done" << std::endl;
    return 0;
}
