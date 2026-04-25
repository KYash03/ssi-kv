// joint balance write skew (smallbank-flavoured but not the canonical pivot):
//
// the literal smallbank pivot in cahill 2008 §5.1 is on a single customer's
// writecheck transaction. that pivot has a ww-conflict on the checking row
// between writecheck and any concurrent transactsavings, so SI's first-
// committer-wins fires before the SSI dangerous-structure check ever runs;
// you can't observe the pivot without three or more transactions and some
// careful schema choices. it's a valid SSI scenario but not a clean two-txn
// demo.
//
// what this demo does instead: a bank policy tying two linked accounts
// (e.g. joint household, two customers) by a sum constraint. each customer's
// writecheck reads both balances and writes only their own row. no ww
// conflict. classic write-skew shape; SSI catches it.

#include "harness.h"

#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

#include <string>

using namespace ssikv;

namespace {
std::string ckey(int cid) { return "c:" + std::to_string(cid) + ":checking"; }

bool seed(txn_manager& tm) {
    auto* t = tm.begin();
    bool ok = tm.write(*t, ckey(1), "60") == status::ok;
    ok &= tm.write(*t, ckey(2), "60") == status::ok;
    ok &= tm.commit(*t) == status::ok;
    return ok;
}

int read_int(txn_manager& tm, transaction& t, const std::string& k) {
    std::string v;
    if (tm.read(t, k, v) != status::ok) return 0;
    return std::stoi(v);
}
} // namespace

int main() {
    store s;
    txn_manager tm(s);
    if (!seed(tm)) return demo::report(false, "joint-balance seed");

    auto* a = tm.begin(); // pays out from customer 1
    auto* b = tm.begin(); // pays out from customer 2

    bool ok = true;
    int a1 = read_int(tm, *a, ckey(1));
    int a2 = read_int(tm, *a, ckey(2));
    int b1 = read_int(tm, *b, ckey(1));
    int b2 = read_int(tm, *b, ckey(2));

    // both check the joint sum >= 100 before paying out 100.
    ok &= (a1 + a2 >= 100);
    ok &= (b1 + b2 >= 100);

    ok &= demo::expect_eq(tm.write(*a, ckey(1), std::to_string(a1 - 100)),
                          status::ok, "a write");
    ok &= demo::expect_eq(tm.write(*b, ckey(2), std::to_string(b2 - 100)),
                          status::ok, "b write");

    ok &= demo::expect_eq(tm.commit(*a), status::ok, "a commit");
    ok &= demo::expect_eq(tm.commit(*b), status::aborted_ssi_dangerous_structure,
                          "b must abort");

    return demo::report(ok, "joint-balance write skew (smallbank-flavoured)");
}
