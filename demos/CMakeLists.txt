# each anomaly demo is its own binary. v1 set lands across commits 15, 24-26, 30-31.
function(add_ssikv_demo name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE ssikv)
endfunction()

# (binaries added in later commits)
