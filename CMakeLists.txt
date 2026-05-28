cmake_minimum_required(VERSION 3.28)
project(curse)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(cmake/compile.cmake)

add_subdirectory(dependencies/gtest)

add_subdirectory(src)
