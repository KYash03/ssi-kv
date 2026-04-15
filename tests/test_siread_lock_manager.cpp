#include <catch_amalgamated.hpp>
#include <ssikv/siread_lock_manager.h>

#include <algorithm>

using namespace ssikv;

TEST_CASE("acquire records holder per page", "[siread]") {
    siread_lock_manager m;
    m.acquire(1, 7);
    m.acquire(2, 7);
    auto h = m.holders_of(7);
    std::sort(h.begin(), h.end());
    REQUIRE(h == std::vector<txn_id_t>{1, 2});
}

TEST_CASE("acquire is idempotent for same txn/page", "[siread]") {
    siread_lock_manager m;
    m.acquire(1, 7);
    m.acquire(1, 7);
    REQUIRE(m.holders_of(7).size() == 1);
}

TEST_CASE("gc reclaims locks of finished txns past oldest_active", "[siread][gc]") {
    siread_lock_manager m;
    m.acquire(1, 7);
    m.acquire(2, 7);
    m.on_finish(1, 100);
    m.on_finish(2, 200);

    // oldest active starts at 150 -> txn 1 (finished at 100) is reclaimable,
    // txn 2 (finished at 200) is not.
    m.gc(150);

    auto h = m.holders_of(7);
    REQUIRE(h.size() == 1);
    REQUIRE(h[0] == 2);
}

TEST_CASE("gc keeps locks of still-running txns", "[siread][gc]") {
    siread_lock_manager m;
    m.acquire(1, 7); // never on_finish
    m.gc(99999);
    REQUIRE(m.holders_of(7).size() == 1);
}
