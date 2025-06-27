include(FetchContent)

# Fetch GoogleTest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG    release-1.12.1
)

# Make GoogleTest available
FetchContent_MakeAvailable(googletest)

# Create a basic test executable
add_executable(gnc_tests main.cpp)

# Link the test executable against gtest and our gnc_lib
target_link_libraries(gnc_tests PRIVATE gtest_main gnc_lib)

# Add the test to CTest with a specific label
include(GoogleTest)
gtest_discover_tests(gnc_tests 
    PROPERTIES LABELS "gnc_project_tests"
    TEST_PREFIX "gnc::"
)
