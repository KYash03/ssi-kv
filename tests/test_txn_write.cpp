#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

TEST_CASE("write buffers into private write-set", "[txn][write]") {
    store s;
    txn_manager tm(s);
    auto* t = tm.begin();
    REQUIRE(tm.write(*t, "k", "v") == status::ok);
    REQUIRE(t->writes.size() == 1);
    REQUIRE(t->writes["k"].has_value());
    REQUIRE(*t->writes["k"] == "v");
}

TEST_CASE("delete buffers a tombstone", "[txn][write]") {
    store s;
    txn_manager tm(s);
    auto* t = tm.begin();
    REQUIRE(tm.del(*t, "k") == status::ok);
    REQUIRE(t->writes.size() == 1);
    REQUIRE_FALSE(t->writes["k"].has_value());
}

TEST_CASE("writes are not visible to a different txn until commit", "[txn][write]") {
    store s;
    txn_manager tm(s);
    auto* a = tm.begin();
    REQUIRE(tm.write(*a, "k", "v") == status::ok);
    // chain still empty; only a's private buffer holds the write.
    REQUIRE(s.find_chain("k") == nullptr);
}
