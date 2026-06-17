
add_library(curse_core 
  types.hpp
  types.cpp

  csv.hpp
  csv.cpp
  
  file_io.hpp
  file_io.cpp


  skippers.hpp
  skippers.cpp

  streams.hpp
  streams.cpp

  storage.hpp
  storage_read.cpp
  storage_write.cpp
  
  aggregators.cpp
  aggregators.hpp
  
  operators.hpp
  constants.hpp
  convert.hpp

  ############# 

  operators/aggregation.hpp  
  operators/aggregation.cpp  

  operators/chain.hpp
  operators/chain.cpp

  operators/count.hpp
  operators/count.cpp

  operators/drop.hpp
  operators/drop.cpp

  operators/enumerate.hpp
  operators/enumerate.cpp

  operators/filter.hpp
  operators/filter.cpp
  
  operators/groupby.hpp
  operators/groupby.cpp
  
  operators/select.hpp
  operators/select.cpp
  
  operators/skip.hpp
  operators/skip.cpp
  
  operators/sort.hpp
  operators/sort.cpp
  
  operators/transform.hpp
  operators/transform.cpp
)
  
target_include_directories(curse_core PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(curse_core PUBLIC curse_util)  
target_link_libraries(curse_core PRIVATE PkgConfig::RE2)
target_link_libraries(curse_core PRIVATE absl::flat_hash_map)

  


file(GLOB TEST_SOURCES "./test/*.cpp")
file(GLOB TEST_OPERATORS_SOURCES "./operators/test/*.cpp")
add_executable(curse_core_test ${TEST_SOURCES} ${TEST_OPERATORS_SOURCES})


target_link_libraries(curse_core_test PUBLIC curse_core GTest::gtest_main)



