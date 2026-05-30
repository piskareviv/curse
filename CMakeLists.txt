cmake_minimum_required(VERSION 3.28)
project(curse)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)



include(cmake/compile.cmake)


add_subdirectory(dependencies/gtest)
add_subdirectory(src)


