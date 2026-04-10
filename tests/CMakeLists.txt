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
)
target_link_libraries(ssikv_tests PRIVATE ssikv catch2)

add_test(NAME ssikv_tests COMMAND ssikv_tests)
