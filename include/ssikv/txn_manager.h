#pragma once

#include <ssikv/status.h>
#include <ssikv/store.h>
#include <ssikv/transaction.h>
#include <ssikv/types.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace ssikv {

class lock_manager;
class siread_lock_manager;

// brain of the engine. owns transactions, hands out timestamps, and runs the
// commit pipeline (FCW first, then dangerous-structure check, then install).
//
// timestamp source is a single seq_cst counter. start_ts and commit_ts share
// the same counter so any commit_ts strictly orders against any concurrent
// start_ts (cahill 2008 §3.2).
class txn_manager {
public:
    explicit txn_manager(store& s) : store_(s), ts_(1) {}
    txn_manager(const txn_manager&) = delete;
    txn_manager& operator=(const txn_manager&) = delete;

    // begin returns a pointer that stays valid until commit/abort. caller does
    // not own; txn_manager retains and frees on terminal state.
    transaction* begin();

    // buffers a write into the txn's private write-set. nothing is visible to
    // other txns until commit installs.
    status write(transaction& t, const key_t& k, const val_t& v);

    // delete is just a write of a tombstone marker.
    status del(transaction& t, const key_t& k);

private:
    [[maybe_unused]] store& store_; // wired up in commits 09+
    std::atomic<ts_t> ts_;

    mutable std::mutex active_mu_;
    std::unordered_map<txn_id_t, std::unique_ptr<transaction>> active_;
    txn_id_t next_id_{1};

    ts_t next_ts() { return ts_.fetch_add(1, std::memory_order_seq_cst); }
};

} // namespace ssikv
