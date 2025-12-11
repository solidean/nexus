cmake_minimum_required(VERSION 3.28)
project(nexus LANGUAGES CXX)

# dependencies
if(SPM_ENABLED)
    spm_require(
        NAME clean-core
        GIT_URL https://github.com/solidean/clean-core.git
        MIN_COMMIT 7cb6f36bc70d54cc16438dfe1b4765b653203195
    )
endif()

# Define the library and its sources
add_library(nexus
    src/nexus/run.cc
    src/nexus/test.cc
    src/nexus/tests/check.cc
    src/nexus/tests/config.cc
    src/nexus/tests/execute.cc
    src/nexus/tests/registry.cc
    src/nexus/tests/schedule.cc
)

# Public headers live co-located in src/ for better editor experience.
# FILE_SET models the public API explicitly (install/export, IDEs, etc.).
target_sources(nexus
    PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES
    src/nexus/fwd.hh
    src/nexus/run.hh
    src/nexus/test.hh
    src/nexus/tests/check.hh
    src/nexus/tests/config.hh
    src/nexus/tests/execute.hh
    src/nexus/tests/registry.hh
    src/nexus/tests/schedule.hh
)

# Libraries should not set a global C++ standard here.
# Instead, declare a minimum on the target so parents stay in control.
# Require at least C++23 for this target; the final -std is chosen by the top-level project.
target_compile_features(nexus
    PUBLIC
    cxx_std_23
)

# Expose "src" so '#include <nexus/dummy.hh>' works for consumers.
target_include_directories(nexus
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(nexus
    PUBLIC
    clean-core
)

# Test executable
add_executable(nexus-test
    tests/main.cc
    tests/test-api-test.cc
    tests/test-registry-test.cc
)

target_link_libraries(nexus-test
    PRIVATE
    nexus
)
