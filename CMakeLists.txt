cmake_minimum_required(VERSION 3.16)
project(GncMetaFramework CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gnc_sim src/main.cpp)

target_include_directories(gnc_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 为不同的编译器设置合适的警告等级
# 这种方式比直接修改 CMAKE_CXX_FLAGS 更加健壮和模块化
if(MSVC)
    # MSVC (Visual Studio) 编译器使用 /W4 开启高级别警告
    target_compile_options(gnc_sim PRIVATE /W4)
else()
    # GCC/Clang 编译器使用 -Wall 和 -Wextra
    # Debug模式下的 -g 标志由 CMake 自动处理，无需手动添加
    target_compile_options(gnc_sim PRIVATE -Wall -Wextra)
endif()