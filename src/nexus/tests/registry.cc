#include "registry.hh"

namespace nx
{
test_registry& get_static_test_registry()
{
    static test_registry registry;
    return registry;
}

} // namespace nx
