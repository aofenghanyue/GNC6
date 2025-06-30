# ============================================================================
# GNC Meta-Framework CMake 构建配置文件
# ============================================================================
# 本文件定义了 GNC (Guidance, Navigation, Control) 元框架的构建配置
# 项目采用模块化设计，将核心功能封装为静态库，便于复用和测试

# 设置 CMake 最低版本要求
cmake_minimum_required(VERSION 3.20)

# 定义项目名称和使用的编程语言
project(GncMetaFramework CXX)

# ============================================================================
# 编译器配置
# ============================================================================
# 设置 C++ 标准为 C++17，确保代码兼容性
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# 第三方库测试禁用配置
# ============================================================================
# 全局禁用所有第三方库的测试构建，避免不必要的编译时间和测试干扰
# 这些设置必须在 FetchContent_MakeAvailable 之前定义才能生效
set(BUILD_TESTING OFF CACHE BOOL "禁用全局测试构建" FORCE)
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "禁用 Eigen 库测试" FORCE)
set(JSON_BuildTests OFF CACHE BOOL "禁用 nlohmann/json 库测试" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "禁用 spdlog 库测试" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "禁用 spdlog 库示例" FORCE)

# 引入 FetchContent 模块，用于自动下载和管理第三方依赖
include(FetchContent)

# 设置第三方库的缓存目录，避免重复下载
# 所有外部依赖将被下载到 deps/ 目录下
set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)

# ============================================================================
# 第三方依赖库配置
# ============================================================================
# 本项目依赖以下第三方库，通过 FetchContent 自动下载和管理

# ----------------------------------------------------------------------------
# Eigen 数学库
# ----------------------------------------------------------------------------
# 用途：提供高性能的线性代数运算，包括矩阵、向量运算等
# 在 GNC 系统中用于状态估计、控制算法等数学密集型计算
FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG        3.4.0  # 使用稳定版本 3.4.0
)
FetchContent_MakeAvailable(eigen)

# ----------------------------------------------------------------------------
# nlohmann/json 库
# ----------------------------------------------------------------------------
# 用途：现代 C++ JSON 解析库，用于配置文件读取、数据序列化等
# 在 GNC 系统中用于参数配置、数据交换等
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.2  # 使用稳定版本 3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# ----------------------------------------------------------------------------
# spdlog 日志库
# ----------------------------------------------------------------------------
# 用途：高性能的 C++ 日志库，支持多种输出格式和日志级别
# 在 GNC 系统中用于调试信息输出、运行状态记录等
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.11.0  # 使用稳定版本 1.11.0
)
FetchContent_MakeAvailable(spdlog)

# ----------------------------------------------------------------------------
# yaml-cpp 库
# ----------------------------------------------------------------------------
# 用途：C++ YAML 解析库，用于支持 YAML 格式的配置文件
# 在 GNC 系统中用于更灵活的配置文件格式，支持注释和更好的可读性
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "禁用 yaml-cpp 库测试" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "禁用 yaml-cpp 工具" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "禁用 yaml-cpp contrib" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "构建 yaml-cpp 静态库" FORCE)
set(YAML_CPP_BUILD_SHARED_LIBS OFF CACHE BOOL "构建 yaml-cpp 静态库" FORCE)
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  # GIT_TAG        yaml-cpp-0.7.0  # 无论是 0.7.0 还是0.8.0版本，都会报错缺少cstdint头文件，默认不带tag的版本没问题
)
FetchContent_MakeAvailable(yaml-cpp)

# ============================================================================
# 自动组件注册配置
# ============================================================================
# 自动扫描 components 目录下的所有组件头文件，生成自动包含文件
# 这样用户只需要编写组件代码，不需要手动在 simulator.cpp 中添加包含语句

# 扫描 components 目录下的所有头文件
file(GLOB_RECURSE COMPONENT_HEADERS "include/gnc/components/*.hpp")

# 设置自动生成的包含文件路径
set(AUTO_INCLUDE_FILE "${CMAKE_CURRENT_BINARY_DIR}/generated/auto_component_includes.hpp")

# 确保生成目录存在
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")

# 生成自动包含文件的内容
set(AUTO_INCLUDE_CONTENT "// Auto-generated file - DO NOT EDIT\n")
set(AUTO_INCLUDE_CONTENT "${AUTO_INCLUDE_CONTENT}// This file automatically includes all component headers for registration\n")
set(AUTO_INCLUDE_CONTENT "${AUTO_INCLUDE_CONTENT}// Generated at build time by CMake\n\n")
set(AUTO_INCLUDE_CONTENT "${AUTO_INCLUDE_CONTENT}#pragma once\n\n")

# 为每个组件头文件添加包含语句
foreach(HEADER ${COMPONENT_HEADERS})
    # 计算相对于项目根目录的路径
    file(RELATIVE_PATH REL_HEADER "${CMAKE_CURRENT_SOURCE_DIR}" "${HEADER}")
    # 转换为包含语句格式
    string(REPLACE "include/" "" INCLUDE_PATH "${REL_HEADER}")
    set(AUTO_INCLUDE_CONTENT "${AUTO_INCLUDE_CONTENT}#include \"${INCLUDE_PATH}\"\n")
endforeach()

# 写入自动生成的包含文件
file(WRITE "${AUTO_INCLUDE_FILE}" "${AUTO_INCLUDE_CONTENT}")

# 输出生成信息
message(STATUS "Generated auto component includes: ${AUTO_INCLUDE_FILE}")
message(STATUS "Found ${CMAKE_MATCH_COUNT} component headers")

# ============================================================================
# GNC 核心库构建配置
# ============================================================================
# 将所有 GNC 核心功能编译为静态库，便于代码复用和模块化管理
# 这种设计使得主程序和测试程序都可以链接同一个库，避免重复编译

# 递归搜索所有 GNC 源文件（头文件和实现文件）
# 包括 include/ 目录下的所有 .hpp 文件和 src/ 目录下的 .cpp 文件
file(GLOB_RECURSE GNC_HEADERS "include/*.hpp")
file(GLOB_RECURSE GNC_SOURCES "src/*.cpp")

# 合并头文件和源文件，包括自动生成的组件包含文件
set(GNC_ALL_SOURCES ${GNC_HEADERS} ${GNC_SOURCES} ${AUTO_INCLUDE_FILE})

# 创建 GNC 静态库
# 静态库的优势：
# 1. 编译一次，多处使用（gnc_sim 和 gnc_tests 都可以链接）
# 2. 依赖关系清晰，便于管理
# 3. 支持独立的单元测试
add_library(gnc_lib STATIC ${GNC_ALL_SOURCES})

# 配置库的头文件搜索路径
target_include_directories(gnc_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include     # GNC 自身的头文件目录
    ${CMAKE_CURRENT_BINARY_DIR}/generated   # 自动生成的文件目录
    ${eigen_SOURCE_DIR}                     # Eigen 数学库（仅头文件库）
    ${nlohmann_json_SOURCE_DIR}/include     # JSON 库（仅头文件库）
    ${spdlog_SOURCE_DIR}/include            # spdlog 日志库头文件
    ${yaml-cpp_SOURCE_DIR}/include          # yaml-cpp 库头文件
)

# 链接第三方库到 GNC 库
# PRIVATE：表示这些依赖关系不会传递给链接 gnc_lib 的其他目标
target_link_libraries(gnc_lib PRIVATE 
    spdlog::spdlog
    yaml-cpp
)

# YAML_CPP_STATIC_DEFINE解决mingw环境下DLL导入警告问题
if(WIN32)
  target_compile_definitions(gnc_lib PUBLIC YAML_CPP_STATIC_DEFINE)
endif()

# ============================================================================
# 主可执行文件构建配置
# ============================================================================
# 构建 GNC 仿真主程序，这是项目的主要入口点

# 创建主可执行文件
# 只需要链接 gnc_lib，所有依赖都通过库传递
add_executable(gnc_sim src/main.cpp)

# 链接 GNC 核心库
# PRIVATE：表示 gnc_lib 的依赖不会暴露给其他目标
target_link_libraries(gnc_sim PRIVATE gnc_lib)

# 设置编译器特定的警告级别
# 启用高级别警告有助于发现潜在的代码问题
if(MSVC)
    # Microsoft Visual C++ 编译器：启用 W4 警告级别
    target_compile_options(gnc_sim PRIVATE /W4)
else()
    # GCC/Clang 编译器：启用常用警告选项
    target_compile_options(gnc_sim PRIVATE -Wall -Wextra)
endif()

# ============================================================================
# 测试框架配置
# ============================================================================
# 配置项目的单元测试，只构建我们自己的测试，不包含第三方库测试

# 提供测试构建选项，用户可以选择是否构建测试
option(GNC_BUILD_TESTING "构建 GNC 项目测试" ON)

if(GNC_BUILD_TESTING)
    # 启用 CTest 测试框架
    # 注意：这里只启用我们项目的测试，第三方库测试已在前面被禁用
    enable_testing()
    
    # 添加测试子目录
    # tests/ 目录包含所有的单元测试代码
    add_subdirectory(tests)
endif()
