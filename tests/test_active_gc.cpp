#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

TEST_CASE("gc_sireads also reaps terminated txn records", "[gc][txns]") {
    store s;
    txn_manager tm(s);

    // create and complete 100 txns. without cleanup the active map would grow
    // monotonically; with cleanup it should bound itself.
    for (int i = 0; i < 100; ++i) {
        auto* t = tm.begin();
        std::string out;
        (void)tm.read(*t, "k", out);
        (void)tm.commit(*t);
    }
    REQUIRE(tm.tracked_count() == 100);

    tm.gc_sireads();
    REQUIRE(tm.tracked_count() == 0); // every txn finished; no active to retain
}

TEST_CASE("gc keeps records of in-flight txns", "[gc][txns]") {
    store s;
    txn_manager tm(s);

    auto* a = tm.begin(); // never finishes
    for (int i = 0; i < 20; ++i) {
        auto* t = tm.begin();
        (void)tm.commit(*t);
    }
    tm.gc_sireads();

    // a is still active so its record stays. all others were younger than a's
    // start_ts so they're retained too (oldest_active = a.start_ts < their
    // finish_ts), so we only assert on a's existence.
    REQUIRE(tm.find_active(a->id) != nullptr);
}
