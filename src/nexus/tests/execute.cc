#include "execute.hh"

#include <nexus/check.hh>

#include <chrono>

namespace nx
{
namespace
{
struct test_context
{
    nx::test_execution* execution = nullptr;
};

thread_local std::vector<test_context> g_context_stack;

void test_execute_begin(nx::test_execution& execution) { g_context_stack.push_back(test_context{.execution = &execution}); }

void test_execute_end() { g_context_stack.pop_back(); }

// Operator to string conversion
char const* op_to_string(impl::cmp_op op)
{
    using namespace impl;
    switch (op)
    {
    case cmp_op::none: return "";
    case cmp_op::less: return "<";
    case cmp_op::less_equal: return "<=";
    case cmp_op::greater: return ">";
    case cmp_op::greater_equal: return ">=";
    case cmp_op::equal: return "==";
    case cmp_op::not_equal: return "!=";
    }
    return "?";
}
} // namespace
} // namespace nx

void nx::impl::report_check_result(check_kind kind, cmp_op op, std::string expr, bool passed, std::vector<std::string> extra_lines, std::source_location location)
{
    if (g_context_stack.empty())
        return; // No active test context

    auto* execution = g_context_stack.back().execution;
    if (!execution)
        return;

    // Increment executed checks
    ++execution->executed_checks;

    // If the check failed, record it
    if (!passed)
    {
        ++execution->failed_checks;

        std::string expanded;
        if (op == cmp_op::none)
            expanded = std::format("is: {}", extra_lines[0]);
        else
            expanded = std::format("is: {} {} {}", extra_lines[0], op_to_string(op), extra_lines[1]);

        // Add test error
        execution->errors.push_back(test_error{
            .expr = std::move(expr),
            .location = location,
            .extra_lines = std::move(extra_lines),
            .expanded = std::move(expanded),
        });
    }
}

bool nx::test_execution::is_considered_failing() const { return failed_checks > 0 || failed_assertions > 0 || !errors.empty(); }

int nx::test_schedule_execution::count_total_tests() const { return static_cast<int>(executions.size()); }

int nx::test_schedule_execution::count_failed_tests() const
{
    int failed = 0;
    for (auto const& exec : executions)
    {
        if (exec.is_considered_failing())
        {
            ++failed;
        }
    }
    return failed;
}

int nx::test_schedule_execution::count_total_checks() const
{
    int total = 0;
    for (auto const& exec : executions)
        total += exec.executed_checks;
    return total;
}

int nx::test_schedule_execution::count_failed_checks() const
{
    int failed = 0;
    for (auto const& exec : executions)
        failed += exec.failed_checks;
    return failed;
}

nx::test_schedule_execution nx::execute_tests(test_schedule const& schedule)
{
    test_schedule_execution result;

    for (auto const& instance : schedule.instances)
    {
        test_execution execution;
        execution.instance = instance;

        // Set up test context for check reporting
        test_execute_begin(execution);

        // Measure test execution time
        auto const start_time = std::chrono::high_resolution_clock::now();

        // Execute the test function if it exists
        if (instance.declaration && instance.declaration->function)
        {
            instance.declaration->function();
        }

        // Calculate duration
        auto const end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> const duration = end_time - start_time;
        execution.duration_seconds = duration.count();

        // Clean up test context
        test_execute_end();

        // Check if test contained any checks
        if (execution.executed_checks == 0 && instance.declaration)
        {
            execution.errors.push_back(test_error{
                .expr = "test did not contain CHECK/REQUIRE",
                .location = instance.declaration->location,
                .extra_lines = {"This is often a bug and can be silenced via CHECK(true)"},
            });
        }

        result.executions.push_back(execution);
    }

    return result;
}
