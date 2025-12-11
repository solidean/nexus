#include "run.hh"

#include <nexus/tests/execute.hh>
#include <nexus/tests/registry.hh>
#include <nexus/tests/schedule.hh>

#include <iostream>

int nx::run(int argc, char** argv)
{
    // Create schedule config from command line arguments
    auto config = test_schedule_config::create_from_args(argc, argv);

    // Get the static test registry
    auto& registry = get_static_test_registry();

    // Create schedule from config and registry
    auto schedule = test_schedule::create(config, registry);

    // Check if any tests were scheduled
    if (schedule.instances.empty())
    {
        std::cerr << "Error: The current schedule did not select any tests\n";
        return 1;
    }

    // Execute the scheduled tests
    auto execution = execute_tests(schedule);

    // Check for failures
    int const failed_tests = execution.count_failed_tests();
    int const total_tests = execution.count_total_tests();

    if (failed_tests > 0)
    {
        // Print failed test information
        std::cerr << "\nFailed tests:\n";
        for (auto const& exec : execution.executions)
        {
            if (exec.is_considered_failing())
            {
                auto const& decl = exec.instance.declaration;
                if (decl)
                {
                    std::cerr << "  " << decl->name << " at " << decl->location.file_name() << ":" << decl->location.line() << "\n";
                }
            }
        }

        std::cerr << "\n" << failed_tests << " of " << total_tests << " tests failed\n";
        return 1;
    }

    // All tests passed
    std::cout << "All " << total_tests << " tests passed\n";
    return 0;
}
