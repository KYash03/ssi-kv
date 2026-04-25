#include <catch_amalgamated.hpp>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

// pivot pattern: T_in -> T -> T_out, both edges rw, all concurrent. T must
// abort with aborted_ssi_dangerous_structure.
//
// schedule, with siread tracking on:
//   T_in begins.        // older snapshot
//   T begins.           // pivot
//   T_in reads x.       // takes siread on x's page
//   T writes y, commits.
//   T_out begins.       // newer txn
//   T_out reads y.      // takes siread on y's page... wait that's the wrong shape
//
// simpler shape, exactly the canonical write-skew minimised:
//   T_in begins.
//   T   begins.
//   T_in reads x.       // siread x.
//   T   reads y.        // siread y.
//   T   writes x, commits.
// at T's commit:
//   - T's siread of y didn't conflict with anything (no writer of y exists).
//   - T's write of x triggers write-side detection: T_in held siread on x ->
//     edge T_in -> T (in_conflict on T).
// then T_in writes y and tries to commit. that triggers:
//   - T_in's write of y triggers write-side: T held siread on y -> edge T -> T_in.
//   - now T_in has BOTH in_conflicts (none yet from elsewhere... hmm).
// this isn't quite the doctors example - let's just construct it more directly.
TEST_CASE("pivot with both incoming and outgoing rw-edges aborts", "[ssi][danger]") {
    store s;
    txn_manager tm(s);

    // three txns. start ordering: a, b, c (so a is the oldest snapshot).
    auto* a = tm.begin();
    auto* b = tm.begin(); // candidate pivot
    auto* c = tm.begin();

    // a reads "x" (siread). b will write "x" -> rw-edge a -> b at b's commit.
    std::string out;
    REQUIRE(tm.read(*a, "x", out) == status::not_found);

    // b reads "y" (siread). c will write "y" -> rw-edge b -> c at c's commit.
    REQUIRE(tm.read(*b, "y", out) == status::not_found);

    // c writes y, commits. detects b's siread on y -> b.out_conflicts += c.
    REQUIRE(tm.write(*c, "y", "v") == status::ok);
    REQUIRE(tm.commit(*c) == status::ok);

    // b now writes x. b's commit detects a's siread on x -> a.out_conflicts +=
    // b, b.in_conflicts += a. b now has BOTH in_conflicts (a) AND
    // out_conflicts (c). dangerous structure -> b aborts.
    REQUIRE(tm.write(*b, "x", "v") == status::ok);
    REQUIRE(tm.commit(*b) == status::aborted_ssi_dangerous_structure);
    REQUIRE_FALSE(b->active());
    REQUIRE(b->abort_reason.starts_with("aborted_ssi_dangerous_structure"));
}

TEST_CASE("only-incoming rw-edge does NOT abort on commit", "[ssi][danger]") {
    store s;
    txn_manager tm(s);
    auto* a = tm.begin();
    auto* b = tm.begin();

    std::string out;
    REQUIRE(tm.read(*a, "x", out) == status::not_found);
    REQUIRE(tm.write(*b, "x", "v") == status::ok);
    REQUIRE(tm.commit(*b) == status::ok); // b only has in_conflicts
}
