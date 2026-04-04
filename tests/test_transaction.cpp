#include <catch_amalgamated.hpp>
#include <ssikv/transaction.h>

using namespace ssikv;

TEST_CASE("default-constructed txn is active and empty", "[txn]") {
    transaction t;
    REQUIRE(t.active());
    REQUIRE(t.id == kInvalidTxn);
    REQUIRE(t.commit_ts == 0);
    REQUIRE(t.writes.empty());
    REQUIRE(t.reads.empty());
    REQUIRE(t.sireads.empty());
    REQUIRE(t.in_conflicts.empty());
    REQUIRE(t.out_conflicts.empty());
}

TEST_CASE("state transitions", "[txn]") {
    transaction t;
    t.st = transaction::state::committed;
    REQUIRE_FALSE(t.active());
    t.st = transaction::state::aborted;
    REQUIRE_FALSE(t.active());
}
