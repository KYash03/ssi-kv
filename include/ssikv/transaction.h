#pragma once

#include <ssikv/types.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ssikv {

// in-flight or terminated txn record. owned by txn_manager.
//
// in/out conflict bookkeeping mirrors ports & grittner 2012 §3.2: we keep full
// lists of concurrent txns we have rw-edges to/from rather than cahill 2008's
// two booleans, because safe-retry (§5.4) needs to know which txns specifically.
struct transaction {
    enum class state {
        active,
        committed,
        aborted,
    };

    txn_id_t id{kInvalidTxn};
    ts_t start_ts{0};
    ts_t commit_ts{0}; // 0 until commit assigns it
    state st{state::active};

    // private write buffer; not visible to other txns until commit installs.
    std::unordered_map<std::string, std::optional<std::string>> writes; // nullopt = tombstone

    // keys this txn has read; used by FCW check at commit.
    std::unordered_set<std::string> reads;

    // page-level siread locks held by this txn. reused for gc bookkeeping.
    std::unordered_set<page_id_t> sireads;

    // rw-antidep edges to/from concurrent txns. populated by txn_manager during
    // read/write. used by the dangerous-structure check at commit (fekete 2005
    // thm 2.1) and by safe-retry (ports & grittner 2012 §5.4).
    std::unordered_set<txn_id_t> in_conflicts;
    std::unordered_set<txn_id_t> out_conflicts;

    // human-readable reason for aborts; surfaced by the wire frontend.
    std::string abort_reason;

    bool active() const { return st == state::active; }
};

} // namespace ssikv
