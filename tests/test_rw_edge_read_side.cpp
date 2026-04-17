#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

TEST_CASE("read past a concurrent committed writer adds rw-edge", "[ssi][rw][read]") {
    store s;
    txn_manager tm(s);

    // a starts FIRST (older snapshot).
    auto* a = tm.begin();

    // b starts later, writes k, commits.
    auto* b = tm.begin();
    REQUIRE(tm.write(*b, "k", "from-b") == status::ok);
    REQUIRE(tm.commit(*b) == status::ok);

    // a now reads k. its snapshot pre-dates b's commit, so it walks past b's
    // version. that walk must record a -> b as rw-antidep.
    std::string out;
    (void)tm.read(*a, "k", out); // not_found is fine; the *edge* is what we test

    REQUIRE(a->out_conflicts.count(b->id) == 1);
    REQUIRE(b->in_conflicts.count(a->id) == 1);
}
