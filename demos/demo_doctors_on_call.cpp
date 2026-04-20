// doctors on call (cahill 2008 fig 1):
//   invariant: at least one doctor is on call. two doctors are currently on
//   call. each doctor opens a txn that checks "is there at least one other on
//   call besides me?", sees yes, and removes themselves. neither sees the
//   other's pending write -> both commit -> nobody on call.
//
// SSI catches it via the rw cycle on the doctors keyspace.

#include "harness.h"

#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

static int count_on_call(txn_manager& tm, transaction& t) {
    int n = 0;
    std::string v;
    if (tm.read(t, "doc:alice", v) == status::ok && v == "on") ++n;
    if (tm.read(t, "doc:bob", v) == status::ok && v == "on") ++n;
    return n;
}

int main() {
    store s;
    txn_manager tm(s);

    // seed: both alice and bob are on call.
    {
        auto* seed = tm.begin();
        (void)tm.write(*seed, "doc:alice", "on");
        (void)tm.write(*seed, "doc:bob", "on");
        (void)tm.commit(*seed);
    }

    auto* alice_txn = tm.begin();
    auto* bob_txn = tm.begin();

    bool ok = true;
    // both doctors check the count and find 2.
    ok &= count_on_call(tm, *alice_txn) == 2;
    ok &= count_on_call(tm, *bob_txn) == 2;

    // each pulls themselves off call.
    ok &= demo::expect_eq(tm.write(*alice_txn, "doc:alice", "off"), status::ok,
                          "alice write");
    ok &= demo::expect_eq(tm.write(*bob_txn, "doc:bob", "off"), status::ok, "bob write");

    // alice commits cleanly.
    ok &= demo::expect_eq(tm.commit(*alice_txn), status::ok, "alice commit");

    // bob's commit triggers the dangerous structure (alice read bob, bob read
    // alice; alice wrote alice, bob wrote bob -> rw both ways).
    ok &= demo::expect_eq(tm.commit(*bob_txn), status::aborted_ssi_dangerous_structure,
                          "bob commit must abort");

    return demo::report(ok, "doctors on call (cahill 2008 fig 1)");
}
