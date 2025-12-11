#pragma once

#include <nexus/config.hh>

#include <source_location>
#include <vector>

namespace nx
{
struct test_declaration
{
    char const* name = nullptr;
    nx::config::cfg test_config;
    void (*function)() = nullptr;
    std::source_location location;
};

struct test_registry
{
    std::vector<test_declaration> declarations;
};

test_registry& get_static_test_registry();

} // namespace nx
