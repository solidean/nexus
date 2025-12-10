#include <nexus/test.hh>

TEST("test api - basic checks")
{
    CHECK(true);
    CHECK(1 < 2);
    CHECK(1 <= 2);
    CHECK(1 + 2 == 3);
    CHECK(1 + 2 != 4);
    CHECK(1 + 3 > 2);
    CHECK(1 + 1 >= 2);
}
