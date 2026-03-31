add_executable(ssikv_app ssikv_main.cpp)
set_target_properties(ssikv_app PROPERTIES OUTPUT_NAME ssikv)
target_link_libraries(ssikv_app PRIVATE ssikv)
