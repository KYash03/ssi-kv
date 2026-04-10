#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>
#include <ssikv/version.h>

using namespace ssikv;

namespace {
// helper: shove a committed version directly into the chain to set up a state.
// real installs go through commit; this is just for read-side tests.
void install(store& s, const std::string& k, ts_t begin, std::string v, txn_id_t creator) {
    auto& chain = s.chain_for(k);
    chain.install(std::make_unique<version>(begin, std::move(v), false, creator));
}
} // namespace

TEST_CASE("read returns not_found for unknown key", "[txn][read]") {
    store s;
    txn_manager tm(s);
    auto* t = tm.begin();
    std::string out;
    REQUIRE(tm.read(*t, "missing", out) == status::not_found);
    REQUIRE(t->reads.count("missing") == 1);
}

TEST_CASE("read sees pre-existing committed version", "[txn][read]") {
    store s;
    install(s, "k", 1, "v1", 100);
    txn_manager tm(s);
    auto* t = tm.begin(); // start_ts >= 2
    std::string out;
    REQUIRE(tm.read(*t, "k", out) == status::ok);
    REQUIRE(out == "v1");
}

TEST_CASE("read does not see versions installed after start_ts", "[txn][read]") {
    store s;
    install(s, "k", 1, "v1", 100); // start state
    txn_manager tm(s);
    auto* t = tm.begin();
    install(s, "k", 999, "v2", 200); // installed after t started
    std::string out;
    REQUIRE(tm.read(*t, "k", out) == status::ok);
    REQUIRE(out == "v1");
}
