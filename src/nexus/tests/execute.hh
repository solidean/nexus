#pragma once

#include <nexus/tests/schedule.hh>

#include <source_location>
#include <string>
#include <vector>


// Forward declaration for impl namespace
namespace nx::impl
{
enum class check_kind;
enum class cmp_op;
} // namespace nx::impl

namespace nx
{
struct test_error
{
    std::string expr;
    std::source_location location;
    std::vector<std::string> extra_lines;
    std::string expanded;
};

struct test_execution
{
    test_instance instance;
    int executed_checks = 0;
    int failed_checks = 0;
    int failed_assertions = 0;
    std::vector<test_error> errors;
    double duration_seconds = 0.0;

    [[nodiscard]] bool is_considered_failing() const;
};

struct test_schedule_execution
{
    std::vector<test_execution> executions;

    [[nodiscard]] int count_total_tests() const;
    [[nodiscard]] int count_failed_tests() const;
    [[nodiscard]] int count_total_checks() const;
    [[nodiscard]] int count_failed_checks() const;
};

test_schedule_execution execute_tests(test_schedule const& schedule, test_schedule_config const& config);

} // namespace nx

namespace nx::impl
{
void report_check_result(check_kind kind, cmp_op op, std::string expr, bool passed, std::vector<std::string> extra_lines, std::source_location location);
}
