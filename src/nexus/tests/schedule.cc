#include "schedule.hh"

nx::test_schedule_config nx::test_schedule_config::create_from_args(int argc, char** argv)
{
    test_schedule_config config;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i)
    {
        config.filters.emplace_back(argv[i]);
    }

    // If filters are provided with non-wildcard matches, enable running disabled tests
    if (!config.filters.empty())
    {
        for (auto const& filter : config.filters)
        {
            // Non-wildcard match (no * character) enables disabled tests
            if (filter.find('*') == std::string::npos)
            {
                config.run_disabled_tests = true;
                break;
            }
        }
    }

    return config;
}

nx::test_schedule nx::test_schedule::create(test_schedule_config const& config, test_registry const& registry)
{
    test_schedule schedule;

    for (auto const& decl : registry.declarations)
    {
        // Skip disabled tests unless explicitly requested
        if (!decl.test_config.enabled && !config.run_disabled_tests)
            continue;

        // Apply filters if any are provided
        if (!config.filters.empty())
        {
            bool matches = false;
            for (auto const& filter : config.filters)
            {
                if (filter.empty())
                    continue;

                // Simple substring match for now
                if (std::string(decl.name).find(filter) != std::string::npos)
                {
                    matches = true;
                    break;
                }
            }

            if (!matches)
                continue;
        }

        // Add test instance to schedule
        schedule.instances.push_back(test_instance{
            .declaration = &decl,
        });
    }

    return schedule;
}
