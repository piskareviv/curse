cmake_minimum_required(VERSION 3.28)

set(CMAKE_C_COMPILER "clang-20")
set(CMAKE_CXX_COMPILER "clang++-20")

project(curse)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)



include(cmake/compile.cmake)


add_subdirectory(dependencies/gtest)
add_subdirectory(dependencies/lz4)
add_subdirectory(src)


