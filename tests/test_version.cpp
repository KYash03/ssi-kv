#include <catch_amalgamated.hpp>
#include <ssikv/version.h>

using namespace ssikv;

TEST_CASE("version is constructed with end_ts at infinity", "[version]") {
    version v{10, "alpha", false, 7};
    REQUIRE(v.begin_ts == 10);
    REQUIRE(v.end_ts == kTsInfinity);
    REQUIRE(v.value == "alpha");
    REQUIRE_FALSE(v.tombstone);
    REQUIRE(v.creator == 7);
    REQUIRE(v.next == nullptr);
}

TEST_CASE("tombstone version carries no value semantics", "[version]") {
    version v{20, "", true, 9};
    REQUIRE(v.tombstone);
    REQUIRE(v.value.empty());
}
