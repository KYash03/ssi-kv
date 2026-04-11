#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

TEST_CASE("commit installs versions visible to later snapshots", "[commit][install]") {
    store s;
    txn_manager tm(s);

    auto* a = tm.begin();
    REQUIRE(tm.write(*a, "k", "v1") == status::ok);
    REQUIRE(tm.commit(*a) == status::ok);

    // a later txn should see "v1" since its start_ts > a.commit_ts.
    auto* b = tm.begin();
    std::string out;
    REQUIRE(tm.read(*b, "k", out) == status::ok);
    REQUIRE(out == "v1");
}

TEST_CASE("a txn that started before the install does NOT see the new version",
          "[commit][install][si]") {
    store s;
    txn_manager tm(s);

    auto* reader = tm.begin();
    auto* writer = tm.begin();

    REQUIRE(tm.write(*writer, "k", "fresh") == status::ok);
    REQUIRE(tm.commit(*writer) == status::ok);

    std::string out;
    REQUIRE(tm.read(*reader, "k", out) == status::not_found); // snapshot stays clean
}

TEST_CASE("delete installs a tombstone", "[commit][install][delete]") {
    store s;
    txn_manager tm(s);
    {
        auto* a = tm.begin();
        REQUIRE(tm.write(*a, "k", "alive") == status::ok);
        REQUIRE(tm.commit(*a) == status::ok);
    }
    {
        auto* a = tm.begin();
        REQUIRE(tm.del(*a, "k") == status::ok);
        REQUIRE(tm.commit(*a) == status::ok);
    }
    auto* b = tm.begin();
    std::string out;
    REQUIRE(tm.read(*b, "k", out) == status::not_found);
}
