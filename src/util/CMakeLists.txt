
add_library(curse_util
  lz4.cpp
  thread_pool.cpp

  ######################
  
  assert.hpp
  hash.hpp
  lz4.hpp
  thread_pool.hpp
  util.hpp
)
  
target_include_directories(curse_util PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(curse_util PRIVATE lz4)  

  


file(GLOB UTIL_TEST_SOURCES "./test/*.cpp")
add_executable(curse_util_test ${UTIL_TEST_SOURCES})


target_link_libraries(curse_util_test PUBLIC curse_util GTest::gtest_main)



