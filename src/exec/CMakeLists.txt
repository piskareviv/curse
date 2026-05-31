
add_executable(convert_csv_to_curse
    convert_csv_to_curse.cpp
)
target_link_libraries(convert_csv_to_curse PRIVATE curse_core)


add_executable(convert_curse_to_csv
    convert_curse_to_csv.cpp
)
target_link_libraries(convert_curse_to_csv PRIVATE curse_core)

