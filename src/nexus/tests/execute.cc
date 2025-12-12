#include "execute.hh"

#include <nexus/tests/check.hh>
#include <nexus/tests/section.hh>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>


namespace nx
{
namespace
{
struct test_section
{
    std::unordered_map<std::string, std::unique_ptr<test_section>> subsections;
    test_section* next_open_section = nullptr;
    bool is_done = false;
    int last_visited_in_exec = -1;
    std::source_location location;
    std::string name;

    void collect_not_done_sections(std::vector<test_section const*>& secs) const
    {
        if (!is_done)
            secs.push_back(this);

        for (auto const& kvp : subsections)
            kvp.second->collect_not_done_sections(secs);
    }
};

struct test_context
{
    nx::test_execution* execution = nullptr;
    std::unique_ptr<test_section> root_section;
    std::vector<test_section*> curr_section;
    int exec_count = 0;
    bool found_leaf = false;
};

// Exception thrown when a REQUIRE fails
struct test_require_failed
{
};

struct test_duplicate_section
{
    std::string name;
    std::source_location location;
};

thread_local std::vector<test_context> g_context_stack;

void test_execute_begin(nx::test_execution& execution)
{
    g_context_stack.push_back(test_context{
        .execution = &execution,
        .root_section = std::make_unique<test_section>(),
    });
    g_context_stack.back().root_section->location = execution.instance.declaration->location;
    g_context_stack.back().curr_section.push_back(g_context_stack.back().root_section.get());
}

void test_execute_end()
{
    g_context_stack.pop_back();
}

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


nx::impl::raii_section_opener nx::impl::test_open_section(std::string name, std::source_location location)
{
    auto& ctx = g_context_stack.back();

    auto& curr_sec = *ctx.curr_section.back();

    // new subsection?
    auto& subsec = curr_sec.subsections[name];
    if (subsec == nullptr)
    {
        subsec = std::make_unique<test_section>();
        subsec->name = name;
        subsec->location = location;
    }

    // section opened twice in the same run
    if (subsec->last_visited_in_exec == ctx.exec_count)
        throw test_duplicate_section{
            .name = std::move(name),
            .location = location,
        };
    subsec->last_visited_in_exec = ctx.exec_count;

    // don't execute more sections if a leaf was already executed
    if (ctx.found_leaf)
    {
        // but note down that parent could continue here
        curr_sec.next_open_section = subsec.get();
        return raii_section_opener(false);
    }

    // don't execute sections that are fully done
    if (subsec->is_done)
        return raii_section_opener(false);

    // .. otherwise enter it
    ctx.curr_section.push_back(subsec.get());
    subsec->next_open_section = nullptr;
    return raii_section_opener(true);
}

nx::impl::raii_section_opener::raii_section_opener(bool is_opened) : _is_opened(is_opened)
{
}

nx::impl::raii_section_opener::~raii_section_opener()
{
    if (_is_opened)
    {
        auto& ctx = g_context_stack.back();
        auto& subsec = *ctx.curr_section.back();

        // if after the section we have no subsecs => found & executed a leaf!
        // (no next open, might have unreachable still)
        // also applies to our way back up
        if (subsec.next_open_section == nullptr)
        {
            ctx.found_leaf = true;
            subsec.is_done = true;
        }
        else
        {
            // make sure parent knows that children have open sections
            ctx.curr_section[ctx.curr_section.size() - 2]->next_open_section = subsec.next_open_section;
        }

        ctx.curr_section.pop_back();
    }
}

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

        // If this was a REQUIRE, throw exception to abort test execution
        if (kind == check_kind::require)
            throw test_require_failed{};
    }
}

bool nx::test_execution::is_considered_failing() const
{
    return failed_checks > 0 || failed_assertions > 0 || !errors.empty();
}

int nx::test_schedule_execution::count_total_tests() const
{
    return static_cast<int>(executions.size());
}

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
            while (true)
            {
                // CAUTION: a test is allowed to run nested tests, thus growing the context stack here
                {
                    auto& ctx = g_context_stack.back();
                    ctx.exec_count++;
                    ctx.found_leaf = false;
                    ctx.root_section->next_open_section = nullptr;
                }

                try
                {
                    (*instance.declaration->function)();
                }
                catch (test_require_failed const&) // NOLINT(bugprone-empty-catch)
                {
                    // REQUIRE failure already logged in report_check_result, this catch
                    // only serves to abort test execution without treating it as a further error
                }
                catch (test_duplicate_section const& e)
                {
                    execution.errors.push_back(test_error{
                        .expr = {},
                        .location = e.location,
                        .extra_lines = {},
                        .expanded = std::format("duplicate section: \"{}\"", e.name),
                    });
                    break; // wrong use of test framework
                }
                catch (std::exception const& e)
                {
                    execution.errors.push_back(test_error{
                        .expr = {},
                        .location = instance.declaration->location,
                        .extra_lines = {},
                        .expanded = std::format("uncaught exception: {}", e.what()),
                    });
                }
                catch (...)
                {
                    execution.errors.push_back(test_error{
                        .expr = {},
                        .location = instance.declaration->location,
                        .extra_lines = {},
                        .expanded = "uncaught unknown exception",
                    });
                }

                // no new sections to execute? we're done
                if (g_context_stack.back().root_section->next_open_section == nullptr)
                {
                    // so it's not marked as unreachable
                    g_context_stack.back().root_section->is_done = true;
                    break;
                }
            }
        }

        // Calculate duration
        auto const end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> const duration = end_time - start_time;
        execution.duration_seconds = duration.count();

        // Check for unreachable sections
        {
            std::vector<test_section const*> unreachable_sections;
            g_context_stack.back().root_section->collect_not_done_sections(unreachable_sections);
            for (auto const& sec : unreachable_sections)
                execution.errors.push_back(test_error{
                    .expr = "",
                    .location = sec->location,
                    .extra_lines = {},
                    .expanded = std::format("section \"{}\" was discovered but unreachable", sec->name),
                });
        }

        // Clean up test context
        test_execute_end();

        // Check if test contained any checks
        // TODO: check for each section
        if (execution.executed_checks == 0 && instance.declaration)
        {
            execution.errors.push_back(test_error{
                .expr = "",
                .location = instance.declaration->location,
                .extra_lines = {"This is often a bug and can be silenced via CHECK(true)"},
                .expanded = "test did not contain CHECK/REQUIRE",
            });
        }

        result.executions.push_back(std::move(execution));
    }

    return result;
}
