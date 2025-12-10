#include <nexus/impl/registration.hh>
#include <nexus/test.hh>

void nx::impl::register_test(char const* name, config::cfg test_config, void (*fn)(), std::source_location loc)
{
    get_test_registry().push_back(test_registration{name, test_config, fn, loc});
}
