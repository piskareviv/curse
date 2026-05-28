add_library(curse_tmp 
    tmp_lib.cpp
)
target_include_directories(curse_tmp INTERFACE ${CMAKE_SOURCE_DIR})


add_executable(curse_tmp_it_works
  cmake_is_working.cpp
)
add_executable(curse_tmp_test
  test/gtest_is_working.cpp
)

target_link_libraries(curse_tmp_it_works PUBLIC curse_tmp)
target_link_libraries(curse_tmp_test PUBLIC curse_tmp gtest_main)

