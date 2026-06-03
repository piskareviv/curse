
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("C++ version: ${CMAKE_CXX_STANDARD}")
 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++ -O2 -Wall -Wextra -Wpedantic -Werror")

set(CMAKE_CXX_FLAGS_ASAN "-O0 -g -fsanitize=address,undefined,bounds -fno-sanitize-recover=all"
    CACHE STRING "Compiler flags in asan build"
    FORCE)


