// tpc-c subset (fekete 2005 §4):
//   fekete et al. proved that tpc-c's full sdg has no dangerous structure,
//   so no execution under SI ever needs to abort for serializability. our
//   ssi must inherit the same property: it should NOT spuriously abort
//   tpc-c-shaped txns. this demo runs a tiny new-order + payment + delivery
//   simulation and asserts every commit succeeds.
//
// this is the false-positive sanity check.

#include "harness.h"

#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

#include <string>

using namespace ssikv;

namespace {
std::string okey(int oid) { return "order:" + std::to_string(oid); }
std::string ckey(int cid) { return "cust:" + std::to_string(cid); }
std::string wkey(int wid) { return "ware:" + std::to_string(wid); }
} // namespace

int main() {
    store s;
    txn_manager tm(s);

    bool ok = true;

    // seed two warehouses, three customers.
    {
        auto* t = tm.begin();
        ok &= tm.write(*t, wkey(1), "ytd=0") == status::ok;
        ok &= tm.write(*t, wkey(2), "ytd=0") == status::ok;
        ok &= tm.write(*t, ckey(1), "bal=100") == status::ok;
        ok &= tm.write(*t, ckey(2), "bal=100") == status::ok;
        ok &= tm.write(*t, ckey(3), "bal=100") == status::ok;
        ok &= tm.commit(*t) == status::ok;
    }

    // new-order for customer 1 in warehouse 1: writes a new order row.
    {
        auto* t = tm.begin();
        std::string v;
        ok &= tm.read(*t, ckey(1), v) == status::ok;
        ok &= tm.read(*t, wkey(1), v) == status::ok;
        ok &= tm.write(*t, okey(101), "items=5") == status::ok;
        ok &= demo::expect_eq(tm.commit(*t), status::ok, "new-order commit");
    }

    // payment for customer 2 in warehouse 1: updates customer balance and
    // warehouse ytd. concurrent with a new-order for customer 3.
    auto* pay = tm.begin();
    auto* neworder = tm.begin();

    {
        std::string v;
        ok &= tm.read(*pay, ckey(2), v) == status::ok;
        ok &= tm.read(*pay, wkey(1), v) == status::ok;
        ok &= tm.write(*pay, ckey(2), "bal=80") == status::ok;
        ok &= tm.write(*pay, wkey(1), "ytd=20") == status::ok;
    }
    {
        std::string v;
        ok &= tm.read(*neworder, ckey(3), v) == status::ok;
        ok &= tm.read(*neworder, wkey(2), v) == status::ok;
        ok &= tm.write(*neworder, okey(102), "items=2") == status::ok;
    }

    ok &= demo::expect_eq(tm.commit(*pay), status::ok, "payment commit");
    ok &= demo::expect_eq(tm.commit(*neworder), status::ok,
                          "concurrent new-order commit");

    // delivery: reads an order, writes a delivery date marker. concurrent
    // with another payment to the same warehouse on a different customer.
    auto* dlv = tm.begin();
    auto* pay2 = tm.begin();

    {
        std::string v;
        ok &= tm.read(*dlv, okey(101), v) == status::ok;
        ok &= tm.write(*dlv, okey(101), "items=5,delivered=t") == status::ok;
    }
    {
        std::string v;
        ok &= tm.read(*pay2, ckey(1), v) == status::ok;
        ok &= tm.read(*pay2, wkey(2), v) == status::ok;
        ok &= tm.write(*pay2, ckey(1), "bal=70") == status::ok;
    }

    ok &= demo::expect_eq(tm.commit(*dlv), status::ok, "delivery commit");
    ok &= demo::expect_eq(tm.commit(*pay2), status::ok, "second payment commit");

    return demo::report(ok, "tpcc subset must not abort");
}
