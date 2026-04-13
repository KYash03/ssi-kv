// lost update (berenson 1995, P4):
//   r1[x=v0] r2[x=v0] w2[x=v1] c2 w1[x=v2] c1
// both txns read x, both intend to write x+something. without protection one
// of the writes silently disappears. SI's first-committer-wins makes the
// second commit abort instead.

#include "harness.h"

#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

int main() {
    store s;
    txn_manager tm(s);

    // seed: x = "0".
    {
        auto* seed = tm.begin();
        (void)tm.write(*seed, "x", "0");
        (void)tm.commit(*seed);
    }

    auto* a = tm.begin();
    auto* b = tm.begin();

    std::string va, vb;
    bool ok = demo::expect_eq(tm.read(*a, "x", va), status::ok, "a read");
    ok &= demo::expect_eq(tm.read(*b, "x", vb), status::ok, "b read");
    ok &= demo::expect_eq(tm.write(*a, "x", "from-a"), status::ok, "a write");
    ok &= demo::expect_eq(tm.write(*b, "x", "from-b"), status::ok, "b write");

    // a wins, b loses (FCW).
    ok &= demo::expect_eq(tm.commit(*a), status::ok, "a commit");
    ok &= demo::expect_eq(tm.commit(*b), status::aborted_si_first_committer_wins,
                          "b commit must abort");

    return demo::report(ok, "lost update aborts under si");
}
