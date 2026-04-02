#pragma once

#include <ssikv/types.h>

#include <memory>

namespace ssikv {

// a single committed version of a key. immutable once installed (commit_ts is
// stamped by txn_manager::commit, never by write).
//
// version order = commit-time order, fekete 2005 §1.1.
struct version {
    ts_t begin_ts;       // commit_ts of creator
    ts_t end_ts;         // kTsInfinity until superseded by a later install
    val_t value;
    bool tombstone;      // true if this version represents a delete
    txn_id_t creator;
    std::unique_ptr<version> next; // older versions; nullptr terminates the chain

    version(ts_t begin, val_t v, bool dead, txn_id_t txn)
        : begin_ts(begin), end_ts(kTsInfinity), value(std::move(v)),
          tombstone(dead), creator(txn), next(nullptr) {}
};

} // namespace ssikv
