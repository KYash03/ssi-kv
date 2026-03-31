#include <catch_amalgamated.hpp>
#include <ssikv/types.h>

using namespace ssikv;

TEST_CASE("type sizes are stable", "[types]") {
    STATIC_REQUIRE(sizeof(txn_id_t) == 8);
    STATIC_REQUIRE(sizeof(ts_t) == 8);
    STATIC_REQUIRE(sizeof(page_id_t) == 8);
    REQUIRE(kInvalidTxn == 0);
    REQUIRE(kTsInfinity > ts_t{1ULL << 60});
}
