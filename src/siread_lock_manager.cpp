#include <ssikv/siread_lock_manager.h>

#include <algorithm>

namespace ssikv {

void siread_lock_manager::acquire(txn_id_t txn, page_id_t page) {
    std::unique_lock lk(mu_);
    auto& set = by_txn_[txn];
    if (set.insert(page).second) {
        locks_[page].push_back(rec{txn, 0});
    }
}

std::vector<txn_id_t> siread_lock_manager::holders_of(page_id_t page) const {
    std::shared_lock lk(mu_);
    std::vector<txn_id_t> out;
    auto it = locks_.find(page);
    if (it == locks_.end()) return out;
    out.reserve(it->second.size());
    for (const auto& r : it->second) {
        out.push_back(r.txn);
    }
    return out;
}

void siread_lock_manager::on_finish(txn_id_t txn, ts_t finish_ts) {
    std::unique_lock lk(mu_);
    auto it = by_txn_.find(txn);
    if (it == by_txn_.end()) return;
    for (page_id_t page : it->second) {
        auto& v = locks_[page];
        for (auto& r : v) {
            if (r.txn == txn) r.finish_ts = finish_ts;
        }
    }
}

void siread_lock_manager::gc(ts_t oldest_active_start_ts) {
    std::unique_lock lk(mu_);
    // we can drop a siread iff its txn has finished AND no still-active txn
    // could be concurrent with it. a sufficient condition: finish_ts <
    // oldest_active_start_ts. cahill 2008 §3.1.
    for (auto pit = locks_.begin(); pit != locks_.end();) {
        auto& v = pit->second;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [&](const rec& r) {
                                   return r.finish_ts != 0 &&
                                          r.finish_ts < oldest_active_start_ts;
                               }),
                v.end());
        if (v.empty()) {
            pit = locks_.erase(pit);
        } else {
            ++pit;
        }
    }
    // by_txn_ entries become irrelevant once their last lock is gone, but
    // walking it here would be O(n); leave it for now and prune on next finish.
}

} // namespace ssikv
