#pragma once

#include <nexus/config.hh>

#include <source_location>
#include <vector>

namespace nx::impl
{
struct test_registration
{
    char const* name;
    nx::config::cfg test_config;
    void (*function)();
    std::source_location location;
};

std::vector<test_registration>& get_test_registry();

} // namespace nx::impl
