
add_library(hits_schema 
    hits_schema.cpp
    hits_schema.hpp
)
  
target_include_directories(hits_schema PRIVATE ${CMAKE_SOURCE_DIR})

# target_include_directories(curse_core PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(convert_csv_to_curse
    convert_csv_to_curse.cpp
)
target_link_libraries(convert_csv_to_curse PRIVATE curse_core hits_schema)


add_executable(convert_curse_to_csv
    convert_curse_to_csv.cpp
)
target_link_libraries(convert_curse_to_csv PRIVATE curse_core)


file(GLOB RUN_QUERY_SOURCES "./queries/*.cpp")

add_executable(run_query
    run_query.cpp
    queries.hpp
    ${RUN_QUERY_SOURCES}
)
target_link_libraries(run_query PRIVATE curse_core hits_schema)


add_executable(tmp_run_query
    tmp_run_query.cpp
)
target_link_libraries(tmp_run_query PRIVATE curse_core hits_schema)

