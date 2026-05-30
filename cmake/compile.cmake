
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message("C++ version: ${CMAKE_CXX_STANDARD}")


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -Werror")

set(CMAKE_CXX_FLAGS_ASAN "-O0 -g -fsanitize=address,undefined,bounds -fno-sanitize-recover=all"
    CACHE STRING "Compiler flags in asan build"
    FORCE)


