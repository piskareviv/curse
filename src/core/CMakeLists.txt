
add_library(curse_core 
  types.cpp
  file_io.cpp
  csv.cpp
  storage.cpp
  lz4.cpp

  #############
  
  types.hpp
  assert.hpp
  csv.hpp
  file_io.hpp
  util.hpp
  csv.hpp
  convert.hpp
  storage.hpp
  constants.hpp
  lz4.hpp
)
  
  target_include_directories(curse_core PUBLIC ${CMAKE_SOURCE_DIR})
  target_link_libraries(curse_core PRIVATE lz4)
  
  
  add_executable(curse_core_test
  test/test_types.cpp
  test/test_util.cpp
  test/test_file_io.cpp
  test/test_csv.cpp
  test/test_convert.cpp
  test/test_storage.cpp
  test/test_lz4.cpp
)


target_link_libraries(curse_core_test PUBLIC curse_core gtest_main)



