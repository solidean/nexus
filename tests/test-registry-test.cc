#include <nexus/test.hh>
#include <nexus/tests/execute.hh>
#include <nexus/tests/registry.hh>
#include <nexus/tests/schedule.hh>


TEST("registry basics")
{
    nx::test_registry reg;
    reg.add_declaration( //
        "testA", {},
        []
        {
            //
            CHECK(1 + 2 == 3);
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule);

    CHECK(exec.count_total_tests() == 1);
    CHECK(exec.count_total_checks() == 1);
    CHECK(exec.count_failed_tests() == 0);
    CHECK(exec.count_failed_checks() == 0);
}
