# asan+ubsan by default in Debug. tsan via a separate build dir:
#   cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DSSIKV_TSAN=ON

option(SSIKV_ASAN "address + undefined sanitizers" ON)
option(SSIKV_TSAN "thread sanitizer (mutually exclusive with asan)" OFF)

add_library(ssikv_sanitizers INTERFACE)
if(SSIKV_TSAN)
    target_compile_options(ssikv_sanitizers INTERFACE -fsanitize=thread -fno-omit-frame-pointer -g)
    target_link_options(ssikv_sanitizers INTERFACE -fsanitize=thread)
elseif(SSIKV_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ssikv_sanitizers INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(ssikv_sanitizers INTERFACE -fsanitize=address,undefined)
endif()
