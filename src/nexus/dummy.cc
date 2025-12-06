#include "dummy.hh"

#include <nexus/test.hh>

int nx::foo() { return 10; }

TEST("foo == 10")
{
    CHECK(nx::foo() == 10);
}
