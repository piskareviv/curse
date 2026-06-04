cmake_minimum_required(VERSION 3.28)

set(CMAKE_C_COMPILER "/usr/bin/clang-20")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++-20")

project(curse)




set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


find_package(PkgConfig REQUIRED)
pkg_check_modules(RE2 REQUIRED IMPORTED_TARGET re2)

find_package(absl REQUIRED)

# find_package(re2 REQUIRED)


include(cmake/compile.cmake)



add_subdirectory(dependencies/gtest)
add_subdirectory(dependencies/lz4)
add_subdirectory(src)


