
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("C++ version: ${CMAKE_CXX_STANDARD}")
 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -Wpedantic -Werror -g")

set(CMAKE_CXX_FLAGS_ASAN "-O0 -g -fsanitize=address,undefined,bounds -fno-sanitize-recover=all"
    CACHE STRING "Compiler flags in asan build"
    FORCE)


set(CMAKE_CXX_FLAGS_TSAN "-g -fsanitize=thread -fno-sanitize-recover=all"
  CACHE STRING "Compiler flags in tsan build"
  FORCE)

set(CMAKE_CXX_FLAGS_MSAN "-g -fsanitize=memory -fsanitize-recover=all"
  CACHE STRING "Compiler flags in msan build"
  FORCE)

set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_ASAN} -fprofile-instr-generate -fcoverage-mapping")

