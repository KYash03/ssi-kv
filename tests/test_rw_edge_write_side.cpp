#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

TEST_CASE("write commit detects siread holders on the same page", "[ssi][rw][write]") {
    store s;
    txn_manager tm(s);

    auto* r = tm.begin();                      // reader, takes siread
    std::string out;
    REQUIRE(tm.read(*r, "k", out) == status::not_found);

    auto* w = tm.begin();                      // writer
    REQUIRE(tm.write(*w, "k", "v") == status::ok);
    REQUIRE(tm.commit(*w) == status::ok);

    // edge r -> w because r held a SIREAD on k's page when w committed.
    REQUIRE(r->out_conflicts.count(w->id) == 1);
    REQUIRE(w->in_conflicts.count(r->id) == 1);
}

TEST_CASE("no rw-edge if writer touches a different page than reader",
          "[ssi][rw][write]") {
    store s;
    txn_manager tm(s);

    auto* r = tm.begin();
    std::string out;
    REQUIRE(tm.read(*r, "alpha", out) == status::not_found);

    auto* w = tm.begin();
    // pick a key whose page differs from "alpha". if they collide on the
    // hash, this test would be a false negative; it's a sanity check.
    if (s.page_for("alpha") == s.page_for("beta")) {
        SUCCEED("hash collision between alpha/beta; skipping");
        return;
    }
    REQUIRE(tm.write(*w, "beta", "v") == status::ok);
    REQUIRE(tm.commit(*w) == status::ok);

    REQUIRE(r->out_conflicts.empty());
    REQUIRE(w->in_conflicts.empty());
}
