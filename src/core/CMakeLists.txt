
add_library(curse_core 
  types.cpp
  file_io.cpp
  csv.cpp
  convert.cpp
  #
  assert.hpp
  csv.hpp
  file_io.hpp
  types.hpp
  util.hpp
  csv.hpp
  convert.hpp
  constants.hpp
)

target_include_directories(curse_core PUBLIC ${CMAKE_SOURCE_DIR})


add_executable(curse_core_test
  test/test_types.cpp
  test/test_util.cpp
  test/test_file_io.cpp
  test/test_csv.cpp
)

target_link_libraries(curse_core_test PUBLIC curse_core gtest_main)



