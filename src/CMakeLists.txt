add_library(ssikv STATIC
    placeholder.cpp
)
target_include_directories(ssikv PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ssikv PUBLIC ssikv_warnings ssikv_sanitizers)
