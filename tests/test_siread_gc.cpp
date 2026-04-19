#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

TEST_CASE("gc_sireads reclaims locks of finished txns when no older active",
          "[ssi][gc]") {
    store s;
    txn_manager tm(s);

    page_id_t p = s.page_for("k");

    auto* a = tm.begin();
    std::string out;
    REQUIRE(tm.read(*a, "k", out) == status::not_found);
    REQUIRE(tm.commit(*a) == status::ok);

    // a finished; no other txns. gc should drop a's siread.
    REQUIRE(tm.sirlocks().holders_of(p).size() == 1);
    tm.gc_sireads();
    REQUIRE(tm.sirlocks().holders_of(p).empty());
}

TEST_CASE("gc_sireads keeps locks if an older txn is still active", "[ssi][gc]") {
    store s;
    txn_manager tm(s);
    page_id_t p = s.page_for("k");

    auto* older = tm.begin(); // never reads or commits
    (void)older;

    auto* a = tm.begin();
    std::string out;
    REQUIRE(tm.read(*a, "k", out) == status::not_found);
    REQUIRE(tm.commit(*a) == status::ok);

    tm.gc_sireads();
    REQUIRE(tm.sirlocks().holders_of(p).size() == 1); // older.start_ts < a.commit
}
