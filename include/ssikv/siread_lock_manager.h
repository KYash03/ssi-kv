#pragma once

#include <ssikv/types.h>

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ssikv {

// page-keyed siread locks. cahill 2008 §3.4: a siread lock records that some
// txn read the page; it does NOT block writers. an rw-antidependency is the
// COMBINATION of an siread held by reader r and a subsequent write by writer w
// to the same page.
//
// retention: siread locks must persist past commit, until every concurrent txn
// has terminated. this is what lets us detect rw-edges where the read happened
// before the write but the write came in after the reader committed (cahill
// 2008 §3.1, last paragraph). gc() reclaims them.
class siread_lock_manager {
public:
    siread_lock_manager() = default;
    siread_lock_manager(const siread_lock_manager&) = delete;
    siread_lock_manager& operator=(const siread_lock_manager&) = delete;

    void acquire(txn_id_t txn, page_id_t page);

    // every txn currently holding a siread on this page.
    std::vector<txn_id_t> holders_of(page_id_t page) const;

    // drop all locks owned by terminated txns whose finish_ts < oldest_active.
    // called periodically from txn_manager.
    void gc(ts_t oldest_active_start_ts);

    // mark a txn as finished so gc() can collect its locks once safe.
    void on_finish(txn_id_t txn, ts_t finish_ts);

private:
    struct rec {
        txn_id_t txn;
        ts_t finish_ts; // 0 while txn is active; set when on_finish() is called
    };

    mutable std::shared_mutex mu_;
    std::unordered_map<page_id_t, std::vector<rec>> locks_;
    std::unordered_map<txn_id_t, std::unordered_set<page_id_t>> by_txn_;
};

} // namespace ssikv
