#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

TEST_CASE("transaction starts with empty conflict lists", "[conflicts]") {
    store s;
    txn_manager tm(s);
    auto* t = tm.begin();
    REQUIRE(t->in_conflicts.empty());
    REQUIRE(t->out_conflicts.empty());
}

TEST_CASE("find_active returns nullptr for unknown id", "[conflicts][active]") {
    store s;
    txn_manager tm(s);
    REQUIRE(tm.find_active(999) == nullptr);
}

TEST_CASE("find_active returns the txn for in-flight ids", "[conflicts][active]") {
    store s;
    txn_manager tm(s);
    auto* t = tm.begin();
    REQUIRE(tm.find_active(t->id).get() == t);
}
