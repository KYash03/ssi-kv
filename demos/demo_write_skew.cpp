// write skew (berenson 1995, A5B):
//   constraint: x + y >= 0. each txn checks the constraint, sees it satisfied,
//   then debits one of the two items. neither sees the other's pending write,
//   so both commit. final state violates the invariant.
//
// SI alone permits this; SSI catches it via the rw-antidep cycle:
//   T1 reads y, writes x.  ->  T2 reads x, writes y.
//   T2 ─rw─> T1 (T2 read x, T1 wrote x)
//   T1 ─rw─> T2 (T1 read y, T2 wrote y)
//   pivot detected at second commit.

#include "harness.h"

#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

int main() {
    store s;
    txn_manager tm(s);

    // seed: x = 50, y = 50. constraint x + y >= 0 holds.
    {
        auto* seed = tm.begin();
        (void)tm.write(*seed, "x", "50");
        (void)tm.write(*seed, "y", "50");
        (void)tm.commit(*seed);
    }

    auto* a = tm.begin();
    auto* b = tm.begin();

    bool ok = true;
    std::string vy_for_a, vx_for_b;

    // a reads y, b reads x. snapshot reads, no conflicts yet.
    ok &= demo::expect_eq(tm.read(*a, "y", vy_for_a), status::ok, "a read y");
    ok &= demo::expect_eq(tm.read(*b, "x", vx_for_b), status::ok, "b read x");

    // a writes x = -100 (assumes y = 50 stays put). b writes y = -100 likewise.
    ok &= demo::expect_eq(tm.write(*a, "x", "-100"), status::ok, "a write x");
    ok &= demo::expect_eq(tm.write(*b, "y", "-100"), status::ok, "b write y");

    // a commits cleanly: only out_conflicts (b read x, hasn't committed).
    ok &= demo::expect_eq(tm.commit(*a), status::ok, "a commit");

    // b's commit detects: rw a -> b (a read y, b writes y) AND rw b -> a (b read
    // x, a wrote x). b has both flags -> dangerous structure -> abort.
    ok &= demo::expect_eq(tm.commit(*b), status::aborted_ssi_dangerous_structure,
                          "b commit must abort under ssi");

    return demo::report(ok, "write skew x+y>=0 aborts under ssi");
}
