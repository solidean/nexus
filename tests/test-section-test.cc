#include <nexus/test.hh>
#include <nexus/tests/config.hh>
#include <nexus/tests/execute.hh>
#include <nexus/tests/registry.hh>
#include <nexus/tests/schedule.hh>

TEST("test sections - basics")
{
    int counterA = 0;
    int counterB = 0;
    int counterC = 0;

    nx::test_registry reg;
    reg.add_declaration( //
        "testA", {},
        [&]
        {
            counterA++;

            SECTION("sec A")
            {
                counterB++;
                CHECK(true);
            }

            SECTION("sec B")
            {
                counterC++;
                CHECK(true);
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule);

    CHECK(counterA == 2);
    CHECK(counterB == 1);
    CHECK(counterC == 1);

    CHECK(exec.count_total_tests() == 1);
    CHECK(exec.count_total_checks() == 2);
    CHECK(exec.count_failed_tests() == 0);
    CHECK(exec.count_failed_checks() == 0);
}
