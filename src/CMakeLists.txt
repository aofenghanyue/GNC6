cmake_minimum_required(VERSION 3.16)
project(GncMetaFramework CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加调试信息和内存检查选项
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra")

add_executable(gnc_sim main.cpp)

target_include_directories(gnc_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})