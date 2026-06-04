
add_executable(test_hashmaps
  test_hashmaps.cpp
)

target_compile_options(test_hashmaps PRIVATE -Wno-error -w)
target_include_directories(test_hashmaps PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_hashmaps PRIVATE absl::flat_hash_map)
