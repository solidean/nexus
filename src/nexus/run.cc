#include "run.hh"

#include <nexus/tests/execute.hh>
#include <nexus/tests/registry.hh>
#include <nexus/tests/schedule.hh>

#include <clean-core/assert.hh>

#include <iostream>

namespace
{
std::string xml_escape(std::string_view str)
{
    std::string result;
    result.reserve(str.size());

    for (auto c : str)
    {
        switch (c)
        {
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '&': result += "&amp;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&apos;"; break;
        default: result += c; break;
        }
    }

    return result;
}

void print_help()
{
    std::cout << "nexus - Unified test, fuzz, benchmark, and app runner for modern C++\n\n";
    // Advertise Catch2 compatibility to enable C++ TestMate IDE extension recognition
    std::cout << "Compatible with Catch2 v3.11.0 in some args\n\n";
    std::cout << "Usage:\n";
    std::cout << "  <test-executable> [options]\n\n";
    std::cout << "For more information, see the nexus documentation.\n";
}

void print_catch2_xml_discovery(nx::test_registry const& registry)
{
    std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    std::cout << "<MatchingTests>\n";

    for (auto const& decl : registry.declarations)
    {
        std::cout << "  <TestCase>\n";
        std::cout << "    <Name>" << xml_escape(decl.name) << "</Name>\n";
        std::cout << "    <ClassName/>\n";
        std::cout << "    <Tags></Tags>\n";
        std::cout << "    <SourceInfo>\n";
        std::cout << "      <File>" << xml_escape(decl.location.file_name()) << "</File>\n";
        std::cout << "      <Line>" << decl.location.line() << "</Line>\n";
        std::cout << "    </SourceInfo>\n";
        std::cout << "  </TestCase>\n";
    }

    std::cout << "</MatchingTests>\n";
}

void print_catch2_execute_result(nx::test_schedule_execution const& execution)
{
    std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    std::cout << "<TestRun>\n";

    for (auto const& exec : execution.executions)
    {
        CC_ASSERT(exec.instance.declaration != nullptr, "test instance is invalid");
        auto const& decl = *exec.instance.declaration;
        bool const success = !exec.is_considered_failing();

        std::cout << "  <TestCase name=\"" << xml_escape(decl.name) << "\" ";
        std::cout << "filename=\"" << xml_escape(decl.location.file_name()) << "\" ";
        std::cout << "line=\"" << decl.location.line() << "\">\n";

        // If test failed, add error expressions (capped)
        if (!success)
        {
            int const max_errors = 50;
            int error_count = 0;
            for (auto const& error : exec.errors)
            {
                if (error_count >= max_errors)
                    break;

                std::cout << "    <Expression success=\"false\" ";
                std::cout << "filename=\"" << xml_escape(error.location.file_name()) << "\" ";
                std::cout << "line=\"" << error.location.line() << "\">\n";
                std::cout << "      <Original>" << xml_escape(error.expr) << "</Original>\n";
                std::cout << "      <Expanded>" << xml_escape(error.expanded) << "</Expanded>\n";
                std::cout << "    </Expression>\n";

                ++error_count;
            }
        }

        std::cout << "    <OverallResult success=\"" << (success ? "true" : "false") << "\" ";
        std::cout << "durationInSeconds=\"" << exec.duration_seconds << "\"/>\n";
        std::cout << "  </TestCase>\n";
    }

    std::cout << "</TestRun>\n";
}
} // namespace

int nx::run(int argc, char** argv)
{
    // Handle --help flag
    if (argc == 2 && std::string_view(argv[1]) == "--help")
    {
        print_help();
        return 0;
    }

    // Create schedule config from command line arguments
    auto config = test_schedule_config::create_from_args(argc, argv);

    // Get the static test registry
    auto& registry = get_static_test_registry();

    // Handle Catch2 XML discovery mode for TestMate integration
    if (config.is_catch2_xml_discovery)
    {
        print_catch2_xml_discovery(registry);
        return 0;
    }

    // Create schedule from config and registry
    auto schedule = test_schedule::create(config, registry);

    // Check if any tests were scheduled
    if (schedule.instances.empty())
    {
        std::cerr << "Error: The current schedule did not select any tests\n";
        for (int i = 0; i < argc; ++i)
        {
            std::cerr << "  arg[" << i << "] = `" << argv[i] << "'\n";
        }
        return 1;
    }

    if (config.verbose)
    {
        schedule.print();
        std::cout << std::endl; // NOLINT
    }

    // Execute the scheduled tests
    auto execution = execute_tests(schedule, config);

    // Handle Catch2 XML results reporting for TestMate integration
    if (config.report_catch2_xml_results)
    {
        print_catch2_execute_result(execution);
        return execution.count_failed_tests() > 0 ? 1 : 0;
    }

    // Check for failures
    int const failed_tests = execution.count_failed_tests();
    int const total_tests = execution.count_total_tests();
    int const failed_checks = execution.count_failed_checks();
    int const total_checks = execution.count_total_checks();

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
                    std::cerr << "  " << decl->name << " at " << decl->location.file_name() << ":"
                              << decl->location.line() << "\n";
                }
            }
        }

        std::cerr << "\n" << failed_tests << " of " << total_tests << " tests failed\n";
        std::cerr << "Failed " << failed_checks << " of " << total_checks << " checks\n";
        return 1;
    }

    // All tests passed
    std::cout << "All " << total_tests << " tests passed (" << total_checks << " checks)\n";
    return 0;
}
