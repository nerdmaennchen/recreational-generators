#include <iostream>

#include "Generator.h"

int main() {
	auto gen = [](auto& t) {
		for (int i{0}; i < 100; ++i) {
			t.yield(i);
		}
	};
	for (auto i : Generator<int>{gen}) {
		std::cout << i << "\n";
	}
	std::cout << "done" << std::endl;
	return 0;
}
