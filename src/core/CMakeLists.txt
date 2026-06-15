
add_library(curse_core 
  types.cpp
  file_io.cpp
  csv.cpp
  storage_read.cpp
  storage_write.cpp
  aggregators.cpp

  #############
  
  types.hpp
  csv.hpp
  file_io.hpp
  csv.hpp
  convert.hpp
  storage.hpp
  constants.hpp
  operators.hpp
  aggregators.hpp


  ############# 

  operators/aggregation.hpp  
  operators/enumerate.hpp
  operators/drop.hpp
  operators/filter.hpp
  operators/groupby.hpp
  operators/select.hpp
  operators/skip.hpp
  operators/sort.hpp
  operators/transform.hpp

  operators/aggregation.cpp  
  operators/enumerate.cpp
  operators/drop.cpp
  operators/filter.cpp
  operators/groupby.cpp
  operators/select.cpp
  operators/skip.cpp
  operators/sort.cpp
  operators/transform.cpp
  operators/count.hpp
  operators/count.cpp
)
  
target_include_directories(curse_core PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(curse_core PUBLIC curse_util)  
target_link_libraries(curse_core PRIVATE PkgConfig::RE2)
target_link_libraries(curse_core PRIVATE absl::flat_hash_map)

  


file(GLOB TEST_SOURCES "./test/*.cpp")
file(GLOB TEST_OPERATORS_SOURCES "./operators/test/*.cpp")
add_executable(curse_core_test ${TEST_SOURCES} ${TEST_OPERATORS_SOURCES})


target_link_libraries(curse_core_test PUBLIC curse_core GTest::gtest_main)



