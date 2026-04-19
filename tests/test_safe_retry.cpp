#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

// safe retry rule (ports & grittner 2012 §5.4): when a dangerous structure
// {T_in -> pivot -> T_out} is detected, we must abort the pivot (the middle
// txn). after retry, the same dangerous structure cannot recur with the same
// T_out because T_out is now committed in the past relative to the retried
// snapshot. our implementation aborts at commit_time of the pivot itself, so
// it is the pivot that always loses, by construction.

TEST_CASE("retry of an aborted pivot succeeds", "[ssi][safe-retry]") {
    store s;
    txn_manager tm(s);

    auto* a = tm.begin();
    auto* b = tm.begin();
    auto* c = tm.begin();

    std::string out;
    REQUIRE(tm.read(*a, "x", out) == status::not_found);
    REQUIRE(tm.read(*b, "y", out) == status::not_found);
    REQUIRE(tm.write(*c, "y", "v") == status::ok);
    REQUIRE(tm.commit(*c) == status::ok);

    REQUIRE(tm.write(*b, "x", "v") == status::ok);
    REQUIRE(tm.commit(*b) == status::aborted_ssi_dangerous_structure);

    // retry b. its new snapshot is taken after c committed, so c is no longer
    // concurrent. the dangerous structure with this c cannot reoccur.
    auto* b2 = tm.begin();
    REQUIRE(tm.write(*b2, "x", "v") == status::ok);
    REQUIRE(tm.commit(*b2) == status::ok);
}
