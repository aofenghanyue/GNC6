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
# 设置 C++ 标准为 C++20，确保代码兼容性
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# 第三方库配置
# ============================================================================
# 这里使用vcpkg直接暗转好相应的库
# 如果使用 conda 环境中预安装的依赖库，而不是 FetchContent 下载
# 确保在激活 gnc-msvc-env conda 环境后运行 CMake

# 设置 CMake 模块路径，用于查找第三方库
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# ============================================================================
# 第三方依赖库配置
# ============================================================================
# 使用vcpkg或使用 conda 环境中预安装的依赖库

# ----------------------------------------------------------------------------
# Eigen 数学库
# ----------------------------------------------------------------------------
# 用途：提供高性能的线性代数运算，包括矩阵、向量运算等
# 在 GNC 系统中用于状态估计、控制算法等数学密集型计算
find_package(Eigen3 REQUIRED)
message(STATUS "Found Eigen3: ${Eigen3_VERSION}")

# ----------------------------------------------------------------------------
# nlohmann/json 库
# ----------------------------------------------------------------------------
# 用途：现代 C++ JSON 解析库，用于配置文件读取、数据序列化等
# 在 GNC 系统中用于参数配置、数据交换等
find_package(nlohmann_json REQUIRED)
message(STATUS "Found nlohmann_json: ${nlohmann_json_VERSION}")

# ----------------------------------------------------------------------------
# spdlog 日志库
# ----------------------------------------------------------------------------
# 用途：高性能的 C++ 日志库，支持多种输出格式和日志级别
# 在 GNC 系统中用于调试信息输出、运行状态记录等
find_package(spdlog REQUIRED)
message(STATUS "Found spdlog: ${spdlog_VERSION}")

# ----------------------------------------------------------------------------
# yaml-cpp 库
# ----------------------------------------------------------------------------
# 用途：C++ YAML 解析库，用于支持 YAML 格式的配置文件
# 在 GNC 系统中用于更灵活的配置文件格式，支持注释和更好的可读性
find_package(yaml-cpp REQUIRED)
message(STATUS "Found yaml-cpp: ${yaml-cpp_VERSION}")

# ----------------------------------------------------------------------------
# HDF5 库
# ----------------------------------------------------------------------------
# 用途：科学数据格式库，用于高效存储和访问大型数据集
# 在 GNC 系统中用于 DataLogger 组件的高效数据记录
option(GNC_ENABLE_HDF5 "Enable HDF5 support" ON)

if(GNC_ENABLE_HDF5)
    # 查找 HDF5 库
    find_package(HDF5 REQUIRED COMPONENTS C CXX)
    
    if(HDF5_FOUND)
        set(HDF5_AVAILABLE ON)
        message(STATUS "Found HDF5: ${HDF5_VERSION}")
        message(STATUS "HDF5 include dirs: ${HDF5_INCLUDE_DIRS}")
        message(STATUS "HDF5 libraries: ${HDF5_LIBRARIES}")
    else()
        set(HDF5_AVAILABLE OFF)
        message(WARNING "HDF5 not found - DataLogger will use CSV format only")
    endif()
else()
    set(HDF5_AVAILABLE OFF)
    message(STATUS "HDF5 support disabled - DataLogger will use CSV format only")
endif()

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
)

# 链接第三方库到 GNC 库
# PUBLIC：表示这些依赖关系会传递给链接 gnc_lib 的其他目标
target_link_libraries(gnc_lib PUBLIC 
    Eigen3::Eigen
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    yaml-cpp::yaml-cpp
)

# 条件性链接 HDF5 库
if(HDF5_AVAILABLE AND GNC_ENABLE_HDF5)
    # 添加 HDF5 头文件路径
    target_include_directories(gnc_lib PUBLIC ${HDF5_INCLUDE_DIRS})
    
    # 链接 HDF5 库
    target_link_libraries(gnc_lib PUBLIC ${HDF5_LIBRARIES})
    
    # 添加 HDF5 编译定义
    target_compile_definitions(gnc_lib PUBLIC 
        HDF5_AVAILABLE
        ${HDF5_DEFINITIONS}
    )
    
    message(STATUS "HDF5 libraries linked to DataLogger")
endif()

# # YAML_CPP_DLL解决mingw环境下DLL导入警告问题
# if(WIN32)
#   target_compile_definitions(gnc_lib PUBLIC YAML_CPP_DLL)
# endif()

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
