#include <nexus/test.hh>
#include <nexus/tests/registry.hh>


void nx::impl::register_test(char const* name, config::cfg test_config, void (*fn)(), std::source_location loc)
{
    nx::get_static_test_registry().declarations.push_back(test_declaration{
        .name = name,
        .test_config = test_config,
        .function = fn,
        .location = loc,
    });
}
