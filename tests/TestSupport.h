#pragma once
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace test
{
struct Case
{
    std::string name;
    std::function<void()> run;
};
inline std::vector<Case> &cases()
{
    static std::vector<Case> all;
    return all;
}
struct Register
{
    Register(const char *n, std::function<void()> f) { cases().push_back({n, std::move(f)}); }
};
inline void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
inline void near(float a, float b, float tolerance = 1.0e-4f)
{
    if (!std::isfinite(a) || !std::isfinite(b) || std::abs(a - b) > tolerance)
        throw std::runtime_error("expected " + std::to_string(b) + ", got " + std::to_string(a));
}
} // namespace test
#define TEST(name)                                                                                           \
    static void name();                                                                                      \
    static test::Register reg_##name(#name, name);                                                           \
    static void name()
#define REQUIRE(x) test::require(static_cast<bool>(x), #x)
