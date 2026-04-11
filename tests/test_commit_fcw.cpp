#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>
#include <ssikv/version.h>

using namespace ssikv;

TEST_CASE("commit aborts FCW when chain has a committed version after start_ts",
          "[commit][fcw]") {
    store s;
    txn_manager tm(s);

    auto* a = tm.begin();
    REQUIRE(tm.write(*a, "k", "from-a") == status::ok);

    // simulate another txn that committed a write to "k" AFTER a started.
    s.chain_for("k").install(
        std::make_unique<version>(a->start_ts + 5, std::string{"intruder"}, false, 999));

    REQUIRE(tm.commit(*a) == status::aborted_si_first_committer_wins);
    REQUIRE_FALSE(a->active());
    REQUIRE(a->abort_reason == "aborted_si_first_committer_wins");
}

TEST_CASE("commit succeeds when no concurrent committed writer", "[commit][fcw]") {
    store s;
    txn_manager tm(s);
    auto* a = tm.begin();
    REQUIRE(tm.write(*a, "k", "v") == status::ok);
    REQUIRE(tm.commit(*a) == status::ok);
    REQUIRE(a->commit_ts > a->start_ts);
}

TEST_CASE("explicit abort marks state and clears locks", "[commit][abort]") {
    store s;
    txn_manager tm(s);
    auto* t = tm.begin();
    REQUIRE(tm.write(*t, "k", "v") == status::ok);
    tm.abort(*t, "user");
    REQUIRE_FALSE(t->active());
    REQUIRE(t->abort_reason == "user");
}
