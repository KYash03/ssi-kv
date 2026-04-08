#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

TEST_CASE("begin returns active txn with monotonic ts and id", "[txn_manager]") {
    store s;
    txn_manager tm(s);

    auto* a = tm.begin();
    auto* b = tm.begin();
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a->active());
    REQUIRE(b->active());
    REQUIRE(a->id != b->id);
    REQUIRE(a->start_ts < b->start_ts);
}
