#include <nexus/test.hh>

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

static std::vector<test_registration>& get_test_registry()
{
    static std::vector<test_registration> registry;
    return registry;
}

} // namespace nx::impl

void nx::impl::register_test(char const* name, config::cfg test_config, void (*fn)(), std::source_location loc)
{
    get_test_registry().push_back(test_registration{name, test_config, fn, loc});
}
