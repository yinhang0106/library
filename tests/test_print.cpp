#include <array>
#include <format>
#include <print.h>
#include <unordered_map>
#include <vector>

int main() {
    std::ostream os{std::cout.rdbuf()};
    std::map<std::string, std::variant<int, double> > m{
        {"hello", 2.1}, {"world", 4}, {"ok", 0}
    };
    print(m);
    std::vector<std::optional<int> > vec{1, 2, std::nullopt, 4, 5};
    print(vec);
    std::string s{"hello"};
    print(s);
    char const *c_str = "world";
    print(c_str);
    std::tuple<int, double, std::string> t{1, 2.1, "hello"};
    print(t);
    char c = 'c';
    print(c);
    char d = 82;
    print(d);
    static_assert(std::is_same_v<decltype(d), char>);
    std::unordered_map<std::string, int> um{{"hello", 42}, {"world", 0}};
    print(um);
    std::array<int, 5> a{1, 2, 3, 4, 5};
    print(a);
    double arr[5] = {1.1, 2.3, 3.8, 4.9, 5.10};
    print(arr);
    auto str = std::format("hello world: {}", to_string(um));
    std::cout << str << std::endl;
}
