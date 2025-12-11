#include "execute.hh"

#include <nexus/check.hh>

namespace
{
struct test_context
{
    nx::test_execution* execution = nullptr;
};

thread_local std::vector<test_context> g_context_stack;

void test_execute_begin(nx::test_execution& execution) { g_context_stack.push_back(test_context{.execution = &execution}); }

void test_execute_end() { g_context_stack.pop_back(); }
} // namespace

void nx::impl::report_check_result(check_kind kind, std::string expr, bool passed, std::vector<std::string> extra_lines, std::source_location location)
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

        // Add test error
        execution->errors.push_back(test_error{
            .expr = std::move(expr),
            .location = location,
            .extra_lines = std::move(extra_lines),
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

nx::test_schedule_execution nx::execute_tests(test_schedule const& schedule)
{
    test_schedule_execution result;

    for (auto const& instance : schedule.instances)
    {
        test_execution execution;
        execution.instance = instance;

        // Set up test context for check reporting
        test_execute_begin(execution);

        // Execute the test function if it exists
        if (instance.declaration && instance.declaration->function)
        {
            instance.declaration->function();
        }

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
