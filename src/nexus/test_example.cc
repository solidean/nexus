#include <nexus/test.hh>

// Example test with no config
TEST("simple test")
{
    // test body
}

// Example test with cfg::disabled
TEST("disabled test", disabled)
{
    // test body
}

// Example test with cfg::seed
TEST("seeded test", seed(42))
{
    // test body
}

// Example test with multiple config items
TEST("complex test", disabled, seed(123))
{
    // test body
}

// Example test with aggregate literal (may not work perfectly yet)
TEST("aggregate test", cfg{.enabled = false, .seed = 999})
{
    // test body
}
