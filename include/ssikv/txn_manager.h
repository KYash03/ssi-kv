#pragma once

#include <ssikv/lock_manager.h>
#include <ssikv/siread_lock_manager.h>
#include <ssikv/status.h>
#include <ssikv/store.h>
#include <ssikv/transaction.h>
#include <ssikv/types.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ssikv {

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
    status write(transaction& t, const std::string& k, const std::string& v);

    // delete is just a write of a tombstone marker.
    status del(transaction& t, const std::string& k);

    // snapshot read at the txn's start_ts. records the key in the read-set so
    // commit-time conflict checks see it. read-your-writes lands in commit 11.
    // returns ok / not_found / err_*.
    status read(transaction& t, const std::string& k, std::string& out);

    // commit goes: FCW check (commit 13) -> dangerous-structure check (commit
    // 21) -> install versions (commit 14). returns aborted_* on failure; the
    // txn is left in aborted state with abort_reason populated.
    status commit(transaction& t);

    // explicit user rollback. always succeeds.
    void abort(transaction& t, std::string_view reason);

    // siread accessors for tests / wire frontend.
    const siread_lock_manager& sirlocks() const { return sirlocks_; }

    // for tests: peek at a still-active transaction by id.
    transaction* find_active(txn_id_t id);

private:
    // record a rw-antidependency from reader to writer. mirrors edges into
    // both txns' conflict lists (ports & grittner 2012 §3.2).
    // caller must hold a graph-lock or equivalent serialization point.
    void add_rw_edge(transaction& reader, transaction& writer);

    store& store_;
    std::atomic<ts_t> ts_;
    lock_manager wlocks_;
    siread_lock_manager sirlocks_;

    mutable std::mutex active_mu_;
    std::unordered_map<txn_id_t, std::unique_ptr<transaction>> active_;
    txn_id_t next_id_{1};

    ts_t next_ts() { return ts_.fetch_add(1, std::memory_order_seq_cst); }
};

} // namespace ssikv
