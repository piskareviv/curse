cmake_minimum_required(VERSION 3.14)
project(curse)

# GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(cmake/compile.cmake)

add_subdirectory(dependencies/gtest)

# enable_testing()
add_subdirectory(src)

