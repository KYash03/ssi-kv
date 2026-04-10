#include <catch_amalgamated.hpp>
#include <ssikv/version_chain.h>

using namespace ssikv;

namespace {
std::unique_ptr<version> make(ts_t begin, std::string v, txn_id_t txn) {
    return std::make_unique<version>(begin, std::move(v), false, txn);
}
} // namespace

TEST_CASE("empty chain has no visible version", "[chain]") {
    version_chain c;
    REQUIRE(c.empty());
    REQUIRE(c.visible_at(100) == nullptr);
}

TEST_CASE("install + visible_at picks latest <= snapshot", "[chain]") {
    version_chain c;
    c.install(make(10, "a", 1));
    c.install(make(20, "b", 2));
    c.install(make(30, "c", 3));

    SECTION("snapshot before any install") {
        REQUIRE(c.visible_at(5) == nullptr);
    }
    SECTION("snapshot at exact commit ts sees that version") {
        const auto* v = c.visible_at(20);
        REQUIRE(v != nullptr);
        REQUIRE(v->value == "b");
    }
    SECTION("snapshot between two installs sees the older one") {
        const auto* v = c.visible_at(25);
        REQUIRE(v != nullptr);
        REQUIRE(v->value == "b");
    }
    SECTION("snapshot after all installs sees the newest") {
        const auto* v = c.visible_at(99);
        REQUIRE(v != nullptr);
        REQUIRE(v->value == "c");
    }
}

TEST_CASE("install sets end_ts on superseded version", "[chain]") {
    version_chain c;
    c.install(make(10, "a", 1));
    c.install(make(20, "b", 2));
    // older version's end_ts becomes newer's begin_ts
    const auto* older = c.visible_at(15);
    REQUIRE(older != nullptr);
    REQUIRE(older->end_ts == 20);
}
