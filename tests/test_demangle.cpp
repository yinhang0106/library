#include <demangle.h>
#include <iostream>
#include <vector>

int main() {
    int a = 42;
    std::cout << demangle<decltype(a)>() << std::endl;
    std::cout << demangle<decltype((a))>() << std::endl;
    std::cout << demangle<decltype(std::as_const(a))>() << std::endl;
    std::cout << demangle<decltype(std::move(a))>() << std::endl;

    auto c_str = "hello";
    std::cout << demangle<decltype(c_str)>() << std::endl;

    std::string s = "world";
    std::cout << demangle<decltype(s)>() << std::endl;

    const std::vector<int> v{1, 2, 3, 4, 5};
    std::cout << demangle<decltype(v)>() << std::endl;

}