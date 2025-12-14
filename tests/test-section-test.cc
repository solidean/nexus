#include <nexus/test.hh>
#include <nexus/tests/config.hh>
#include <nexus/tests/execute.hh>
#include <nexus/tests/registry.hh>
#include <nexus/tests/schedule.hh>

#include <format>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    auto exec = nx::execute_tests(schedule, {});

    CHECK(counterA == 2);
    CHECK(counterB == 1);
    CHECK(counterC == 1);

    CHECK(exec.count_total_tests() == 1);
    CHECK(exec.count_total_checks() == 2);
    CHECK(exec.count_failed_tests() == 0);
    CHECK(exec.count_failed_checks() == 0);
}

TEST("test sections - basics nested")
{
    int counterA = 0;
    int counterB = 0;
    int counterC = 0;

    nx::test_registry reg;
    reg.add_declaration( //
        "testA", {},
        [&]
        {
            SECTION("outer")
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
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule, {});

    CHECK(counterA == 2);
    CHECK(counterB == 1);
    CHECK(counterC == 1);

    CHECK(exec.count_total_tests() == 1);
    CHECK(exec.count_total_checks() == 2);
    CHECK(exec.count_failed_tests() == 0);
    CHECK(exec.count_failed_checks() == 0);
}

TEST("test sections - canonical preorder + counts on a richer tree")
{
    int counterRoot = 0;
    int counterA = 0;
    int counterA1 = 0;
    int counterA2 = 0;
    int counterB = 0;
    int counterB1 = 0;
    int counterC = 0;
    std::vector<int> log;

    nx::test_registry reg;
    reg.add_declaration( //
        "testMultiLevel", {},
        [&]
        {
            counterRoot++;

            SECTION("A")
            {
                counterA++;
                log.push_back(1);

                SECTION("A1")
                {
                    counterA1++;
                    log.push_back(2);
                    CHECK(true);
                }

                SECTION("A2")
                {
                    counterA2++;
                    log.push_back(3);
                    CHECK(true);
                }
            }

            SECTION("B")
            {
                counterB++;
                log.push_back(4);

                SECTION("B1")
                {
                    counterB1++;
                    log.push_back(5);
                    CHECK(true);
                }
            }

            SECTION("C")
            {
                counterC++;
                log.push_back(6);
                CHECK(true);
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule, {});

    CHECK(counterRoot == 4);
    CHECK(counterA == 2);
    CHECK(counterA1 == 1);
    CHECK(counterA2 == 1);
    CHECK(counterB == 1);
    CHECK(counterB1 == 1);
    CHECK(counterC == 1);

    // Each run executes from root to leaf, so log is interleaved:
    // Run 1: root -> A -> A1: logs 1, 2
    // Run 2: root -> A -> A2: logs 1, 3
    // Run 3: root -> B -> B1: logs 4, 5
    // Run 4: root -> C: logs 6
    auto const expectedLog = std::vector<int>{1, 2, 1, 3, 4, 5, 6};
    CHECK(log == expectedLog);

    CHECK(exec.count_total_tests() == 1);
    CHECK(exec.count_total_checks() == 4);
    CHECK(exec.count_failed_tests() == 0);
    CHECK(exec.count_failed_checks() == 0);
}

TEST("test sections - distinct dynamic sections in a loop, preorder stable")
{
    int N = 4;
    int counterRoot = 0;
    std::vector<int> log;

    nx::test_registry reg;
    reg.add_declaration( //
        "testDynamicLoop", {},
        [&]
        {
            counterRoot++;

            for (int i = 0; i < N; ++i)
            {
                SECTION("item {}", i)
                {
                    log.push_back(i);
                    CHECK(true);
                }
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule, {});

    CHECK(counterRoot == N);

    auto const expectedLog = std::vector<int>{0, 1, 2, 3};
    CHECK(log == expectedLog);

    CHECK(exec.count_total_tests() == 1);
    CHECK(exec.count_total_checks() == N);
    CHECK(exec.count_failed_tests() == 0);
    CHECK(exec.count_failed_checks() == 0);
}

TEST("test sections - dynamic loop with nested subsections, preorder and counts")
{
    int N = 3;
    int counterRoot = 0;
    std::vector<std::pair<int, std::string>> log;

    nx::test_registry reg;
    reg.add_declaration( //
        "testDynamicNested", {},
        [&]
        {
            counterRoot++;

            for (int i = 0; i < N; ++i)
            {
                SECTION("i={}", i)
                {
                    SECTION("even")
                    {
                        log.emplace_back(i, "even");
                        CHECK(true);
                    }

                    SECTION("odd")
                    {
                        log.emplace_back(i, "odd");
                        CHECK(true);
                    }
                }
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule, {});

    CHECK(counterRoot == 2 * N);

    // Each run executes from root through parent section to leaf:
    // Run for i=0/even, then i=0/odd, then i=1/even, then i=1/odd, etc.
    auto const expected = std::vector<std::pair<int, std::string>>{
        {0, "even"}, {0, "odd"}, {1, "even"}, {1, "odd"}, {2, "even"}, {2, "odd"},
    };
    CHECK(log == expected);

    CHECK(exec.count_total_tests() == 1);
    CHECK(exec.count_total_checks() == 2 * N);
    CHECK(exec.count_failed_tests() == 0);
    CHECK(exec.count_failed_checks() == 0);
}

TEST("test sections - conditionally active subsections across runs (all reachable)")
{
    int counterRoot = 0;
    std::vector<std::string> log;

    nx::test_registry reg;
    reg.add_declaration( //
        "testConditional", {},
        [&, phase = 0]() mutable
        {
            counterRoot++;

            SECTION("outer")
            {
                if (phase == 0)
                {
                    SECTION("first")
                    {
                        log.emplace_back("first");
                        phase = 1;
                        CHECK(true);
                    }
                }
                else
                {
                    SECTION("second")
                    {
                        log.emplace_back("second");
                        CHECK(true);
                    }
                }

                // NOTE: we need another section here so we don't close outer early
                //       this "ultra" dynamic discovery is not super useful
                SECTION("third")
                {
                    log.emplace_back("third");
                    CHECK(true);
                }
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule, {});

    CHECK(counterRoot == 3);

    auto const expected = std::vector<std::string>{
        "first",
        "second",
        "third",
    };
    CHECK(log == expected);

    CHECK(exec.count_total_tests() == 1);
    CHECK(exec.count_total_checks() == 3);
    CHECK(exec.count_failed_tests() == 0);
    CHECK(exec.count_failed_checks() == 0);
}

TEST("test sections - discovered-but-vanishing subsection => failure but no infinite loop")
{
    int visitsOuter = 0;
    int visitsOnce = 0;
    int visitsVanish = 0;

    nx::test_registry reg;
    reg.add_declaration( //
        "testVanishing", {},
        [&, flag = false]() mutable
        {
            SECTION("outer")
            {
                ++visitsOuter;

                if (!flag)
                {
                    SECTION("once")
                    {
                        ++visitsOnce;
                        flag = true;
                        CHECK(true);
                    }

                    SECTION("vanish")
                    {
                        ++visitsVanish;
                        CHECK(true);
                    }
                }
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule, {});

    CHECK(visitsOuter == 2);
    CHECK(visitsOnce == 1);
    CHECK(visitsVanish == 0);

    CHECK(exec.count_failed_tests() == 1);
}

TEST("test sections - duplicate sibling section names => immediate error")
{
    int first = 0;
    int second = 0;

    nx::test_registry reg;
    reg.add_declaration( //
        "testDuplicate", {},
        [&]
        {
            SECTION("parent")
            {
                SECTION("dup")
                {
                    ++first;
                    CHECK(true);
                }

                SECTION("dup")
                {
                    ++second;
                    CHECK(true);
                }
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule, {});

    CHECK(first == 1);
    CHECK(second == 0);

    CHECK(exec.count_total_checks() == 1);
    CHECK(exec.count_failed_tests() == 1);
}

TEST("test sections - failure in one leaf still allows siblings to run")
{
    std::vector<std::string> log;

    nx::test_registry reg;
    reg.add_declaration( //
        "testNonFatalFailure", {},
        [&]
        {
            SECTION("Root")
            {
                SECTION("A")
                {
                    log.emplace_back("A");
                    CHECK(false);
                }

                SECTION("B")
                {
                    log.emplace_back("B");
                    CHECK(true);
                }
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule, {});

    auto const expected = std::vector<std::string>{"A", "B"};
    CHECK(log == expected);

    CHECK(exec.count_total_tests() == 1);
    CHECK(exec.count_total_checks() == 2);
    CHECK(exec.count_failed_tests() == 1);
    CHECK(exec.count_failed_checks() == 1);
}

TEST("test sections - exception inside one leaf doesn't corrupt scheduling")
{
    std::vector<std::string> log;

    nx::test_registry reg;
    reg.add_declaration( //
        "testException", {},
        [&]
        {
            SECTION("Root")
            {
                SECTION("throws")
                {
                    log.emplace_back("throws_enter");
                    throw std::runtime_error("test exception");
                }

                SECTION("ok")
                {
                    log.emplace_back("ok_enter");
                    CHECK(true);
                }
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule, {});

    auto const expected = std::vector<std::string>{
        "throws_enter",
        // "ok_enter", -> for now, siblings are not guaranteed to be visited
    };
    CHECK(log == expected);

    CHECK(exec.count_failed_tests() == 1);
}

TEST("test sections - early-abort-style macro (REQUIRE) terminates path but not schedule")
{
    int afterRequire = 0;
    int visitedOk = 0;

    nx::test_registry reg;
    reg.add_declaration( //
        "testRequire", {},
        [&]
        {
            SECTION("Root")
            {
                SECTION("fatal")
                {
                    REQUIRE(false);
                    ++afterRequire;
                }

                SECTION("ok")
                {
                    ++visitedOk;
                    CHECK(true);
                }
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule, {});

    CHECK(afterRequire == 0);
    CHECK(visitedOk == 0); // NOTE: for now, siblings might be visited

    CHECK(exec.count_failed_tests() == 1);
}

TEST("test sections - nested conditionals with subsections active at different times")
{
    int N = 2;
    int counterRoot = 0;
    std::vector<std::string> log;

    nx::test_registry reg;
    reg.add_declaration( //
        "testNestedConditional", {},
        [&]
        {
            counterRoot++;

            for (int i = 0; i < N; ++i)
            {
                SECTION("i={}", i)
                {
                    if (i == 0)
                    {
                        SECTION("X")
                        {
                            log.emplace_back("i0/X");
                            CHECK(true);
                        }
                    }
                    else
                    {
                        SECTION("Y")
                        {
                            log.emplace_back("i1/Y");
                            CHECK(true);
                        }
                    }
                }
            }
        });

    auto schedule = nx::test_schedule::create({}, reg);
    auto exec = nx::execute_tests(schedule, {});

    CHECK(counterRoot == 2);

    auto const expected = std::vector<std::string>{
        "i0/X",
        "i1/Y",
    };
    CHECK(log == expected);

    CHECK(exec.count_total_tests() == 1);
    CHECK(exec.count_total_checks() == 2);
    CHECK(exec.count_failed_tests() == 0);
    CHECK(exec.count_failed_checks() == 0);
}
