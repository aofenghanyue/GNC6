cmake_minimum_required(VERSION 3.16)
project(GncMetaFramework CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Globally disable testing for all third-party libraries
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)

include(FetchContent)

# Set the base directory for FetchContent to cache external projects
set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)

# --- Third-party Libraries ---

# Eigen
FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG        3.4.0
)
FetchContent_MakeAvailable(eigen)

# JSON for Modern C++ (nlohmann/json)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# spdlog
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.11.0
)
FetchContent_MakeAvailable(spdlog)

# --- GNC Library ---
# Glob all source files for the library
file(GLOB_RECURSE GNC_SOURCES "src/gnc/*.hpp" "src/gnc/*.cpp")

# Create a static library from the GNC sources
add_library(gnc_lib STATIC ${GNC_SOURCES})

# Add the src directory to the include path for the library
target_include_directories(gnc_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EIGEN_INCLUDE_DIR} # Eigen is header-only
    ${nlohmann_json_SOURCE_DIR}/include # JSON is header-only
    ${spdlog_SOURCE_DIR}/include # spdlog include directory
)

target_link_libraries(gnc_lib PRIVATE spdlog::spdlog)

# --- Main Executable ---
add_executable(gnc_sim src/main.cpp)
target_link_libraries(gnc_sim PRIVATE gnc_lib)

# Set compiler-specific warning levels
if(MSVC)
    target_compile_options(gnc_sim PRIVATE /W4)
else()
    target_compile_options(gnc_sim PRIVATE -Wall -Wextra)
endif()

# --- Testing ---
# Enable testing only for our project
option(GNC_BUILD_TESTING "Build GNC tests" ON)

if(GNC_BUILD_TESTING)
    # Enable testing framework but only for our tests
    enable_testing()
    
    # Add the tests directory
    add_subdirectory(tests)
endif()
