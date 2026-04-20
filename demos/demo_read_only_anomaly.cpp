// read-only anomaly (fekete 2005 ex 1.3 / fekete et al. 2004):
//   x = 0, y = 0 initially.
//   t2 reads x, reads y, decides whether to apply an overdraft fee, writes x.
//   t1 deposits 20 to y.
//   t3 (read-only) reads x and y for a customer report.
//
//   schedule: t2 reads x=0, y=0. t1 writes y=20 commits. t3 reads x=0, y=20
//   commits. t2 writes x=-11 commits.
//
//   t3's printed state {x=0, y=20} cannot occur in any serial order whose
//   final state matches: in any serial order with t2 last, the overdraft
//   should not fire because t1 already added 20. SSI catches this even
//   though t3 is read-only — t2 is the pivot.

#include "harness.h"

#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

int main() {
    store s;
    txn_manager tm(s);
    {
        auto* seed = tm.begin();
        (void)tm.write(*seed, "x", "0");
        (void)tm.write(*seed, "y", "0");
        (void)tm.commit(*seed);
    }

    // t2 starts first and reads its snapshot of x and y.
    auto* t2 = tm.begin();
    std::string vx, vy;
    bool ok = true;
    ok &= demo::expect_eq(tm.read(*t2, "x", vx), status::ok, "t2 read x");
    ok &= demo::expect_eq(tm.read(*t2, "y", vy), status::ok, "t2 read y");

    // t1 deposits 20 to y and commits.
    auto* t1 = tm.begin();
    ok &= demo::expect_eq(tm.write(*t1, "y", "20"), status::ok, "t1 write y");
    ok &= demo::expect_eq(tm.commit(*t1), status::ok, "t1 commit");

    // t3 (read-only) reads after t1 committed -> sees y=20 but x=0.
    auto* t3 = tm.begin();
    ok &= demo::expect_eq(tm.read(*t3, "x", vx), status::ok, "t3 read x");
    ok &= demo::expect_eq(tm.read(*t3, "y", vy), status::ok, "t3 read y");
    ok &= demo::expect_eq(tm.commit(*t3), status::ok, "t3 commit");

    // t2 now writes x=-11. its commit detects rw cycle through t3 and t1
    // (t2 -> t1 via y, t3 -> t2 via x) -> dangerous structure -> abort.
    ok &= demo::expect_eq(tm.write(*t2, "x", "-11"), status::ok, "t2 write x");
    ok &= demo::expect_eq(tm.commit(*t2), status::aborted_ssi_dangerous_structure,
                          "t2 commit must abort");

    return demo::report(ok, "read-only anomaly (fekete 2005 ex 1.3)");
}
