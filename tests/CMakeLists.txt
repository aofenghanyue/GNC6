
include(FetchContent)

# Fetch GoogleTest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG    release-1.12.1
)

# Make GoogleTest available
FetchContent_MakeAvailable(googletest)

# 添加测试可执行文件
add_executable(gnc_tests
    test_config_manager.cpp
    test_transform.cpp
)

# 链接库
target_link_libraries(gnc_tests
    gtest_main
    gnc_lib
)

# 确保测试目标能访问到所有必要的头文件
target_include_directories(gnc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 发现测试
include(GoogleTest)
gtest_discover_tests(gnc_tests)