# each anomaly demo is its own binary. v1 set lands across commits 15, 24-26, 30-31.
function(add_ssikv_demo name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE ssikv)
endfunction()

add_ssikv_demo(demo_lost_update)
add_ssikv_demo(demo_write_skew)
add_ssikv_demo(demo_doctors_on_call)
add_ssikv_demo(demo_read_only_anomaly)
add_ssikv_demo(demo_smallbank)
