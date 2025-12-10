#include "registration.hh"

#include <vector>

namespace nx::impl
{
std::vector<test_registration>& get_test_registry()
{
    static std::vector<test_registration> registry;
    return registry;
}

} // namespace nx::impl
