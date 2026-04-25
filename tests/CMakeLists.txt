add_library(catch2 STATIC ${CMAKE_SOURCE_DIR}/third_party/catch2/catch_amalgamated.cpp)
# SYSTEM include silences warnings from catch2 headers (we keep -Werror for our own code).
target_include_directories(catch2 SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third_party/catch2)
# catch2 amalgamated provides its own main; tests link in and write TEST_CASEs only.

add_executable(ssikv_tests
    test_smoke.cpp
    test_types.cpp
    test_status.cpp
    test_version.cpp
    test_version_chain.cpp
    test_store.cpp
    test_transaction.cpp
    test_txn_manager_begin.cpp
    test_txn_write.cpp
    test_txn_read.cpp
    test_lock_manager.cpp
    test_commit_fcw.cpp
    test_commit_install.cpp
    test_siread_lock_manager.cpp
    test_siread_on_read.cpp
    test_conflict_lists.cpp
    test_rw_edge_read_side.cpp
    test_rw_edge_write_side.cpp
    test_dangerous_structure.cpp
    test_safe_retry.cpp
    test_siread_gc.cpp
    test_repl.cpp
    test_concurrent.cpp
    test_active_gc.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(ssikv_tests PRIVATE Threads::Threads)
target_link_libraries(ssikv_tests PRIVATE ssikv catch2)

add_test(NAME ssikv_tests COMMAND ssikv_tests)
