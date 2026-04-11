#pragma once

#include <ssikv/types.h>

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ssikv {

// short-duration write locks. used by commit() to enforce first-committer-wins
// (berenson 1995 §4.2): a txn that wants to commit must hold a write lock on
// every key in its write-set, and there must be no committed concurrent writer
// to that key.
//
// these are NOT siread locks; siread is its own type with very different
// retention rules (cahill 2008 §3.4).
class lock_manager {
public:
    lock_manager() = default;
    lock_manager(const lock_manager&) = delete;
    lock_manager& operator=(const lock_manager&) = delete;

    // try to acquire write locks on all keys for txn. all-or-nothing: if any
    // key is already held by another txn, releases anything just acquired and
    // returns false.
    bool try_acquire_all(txn_id_t txn, const std::vector<std::string>& keys);

    void release_all(txn_id_t txn);

    std::optional<txn_id_t> holder(const std::string& k) const;

private:
    mutable std::mutex mu_;
    std::unordered_map<std::string, txn_id_t> holders_;
};

} // namespace ssikv
