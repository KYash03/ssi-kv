#include <catch_amalgamated.hpp>
#include <ssikv/lock_manager.h>

using namespace ssikv;

TEST_CASE("free key acquires", "[locks]") {
    lock_manager lm;
    REQUIRE(lm.try_acquire_all(1, {"a", "b"}));
    REQUIRE(lm.holder("a") == 1);
    REQUIRE(lm.holder("b") == 1);
}

TEST_CASE("conflicting acquire fails atomically", "[locks]") {
    lock_manager lm;
    REQUIRE(lm.try_acquire_all(1, {"a"}));
    // txn 2 wants {"b", "a"}; "a" is held -> whole acquire must fail and "b"
    // must NOT be left held.
    REQUIRE_FALSE(lm.try_acquire_all(2, {"b", "a"}));
    REQUIRE(lm.holder("b") == std::nullopt);
}

TEST_CASE("release_all only drops the txn's own keys", "[locks]") {
    lock_manager lm;
    REQUIRE(lm.try_acquire_all(1, {"a"}));
    REQUIRE(lm.try_acquire_all(2, {"b"}));
    lm.release_all(1);
    REQUIRE(lm.holder("a") == std::nullopt);
    REQUIRE(lm.holder("b") == 2);
}

TEST_CASE("re-acquire by same txn is idempotent", "[locks]") {
    lock_manager lm;
    REQUIRE(lm.try_acquire_all(1, {"a"}));
    REQUIRE(lm.try_acquire_all(1, {"a", "b"}));
    REQUIRE(lm.holder("a") == 1);
    REQUIRE(lm.holder("b") == 1);
}
