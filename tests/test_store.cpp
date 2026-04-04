#include <catch_amalgamated.hpp>
#include <ssikv/store.h>

using namespace ssikv;

TEST_CASE("store creates a chain on first touch", "[store]") {
    store s;
    REQUIRE(s.find_chain("missing") == nullptr);
    auto& c = s.chain_for("alpha");
    REQUIRE(c.empty());
    REQUIRE(s.find_chain("alpha") == &c);
}

TEST_CASE("page_for is deterministic and bounded", "[store]") {
    store s;
    page_id_t p = s.page_for("foo");
    REQUIRE(p < store::kNumPages);
    REQUIRE(s.page_for("foo") == p); // stable
}

TEST_CASE("chain_for returns the same chain on second call", "[store]") {
    store s;
    auto& a = s.chain_for("k");
    auto& b = s.chain_for("k");
    REQUIRE(&a == &b);
}
