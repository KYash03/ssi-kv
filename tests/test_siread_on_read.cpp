#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

TEST_CASE("read takes a siread on the page containing the key", "[siread][read]") {
    store s;
    txn_manager tm(s);
    auto* t = tm.begin();
    std::string out;
    REQUIRE(tm.read(*t, "k", out) == status::not_found);

    page_id_t p = s.page_for("k");
    auto holders = tm.sirlocks().holders_of(p);
    REQUIRE(holders.size() == 1);
    REQUIRE(holders[0] == t->id);
    REQUIRE(t->sireads.count(p) == 1);
}

TEST_CASE("two reads on different keys may share or split pages", "[siread][read]") {
    store s;
    txn_manager tm(s);
    auto* t = tm.begin();
    std::string out;
    REQUIRE(tm.read(*t, "alpha", out) == status::not_found);
    REQUIRE(tm.read(*t, "beta", out) == status::not_found);
    // both pages must include this txn as a holder. set membership is enough.
    REQUIRE(tm.sirlocks().holders_of(s.page_for("alpha")).size() == 1);
    REQUIRE(tm.sirlocks().holders_of(s.page_for("beta")).size() == 1);
}
