#pragma once

#include <ssikv/types.h>
#include <ssikv/version.h>

#include <memory>
#include <optional>
#include <shared_mutex>
#include <utility>

namespace ssikv {

// per-key chain of immutable versions, head is newest. installs are head-pushes.
//
// snapshot reads walk back until begin_ts <= snapshot. ts equality means the
// txn's snapshot is taken AFTER the install committed, so it sees the version.
//
// concurrency: shared_mutex; readers hold shared, install holds exclusive. while
// a reader holds the shared lock the chain cannot be freed under it (the chain
// owns the unique_ptrs and only mutates head_ under the exclusive lock).
class version_chain {
public:
    version_chain() = default;
    version_chain(const version_chain&) = delete;
    version_chain& operator=(const version_chain&) = delete;
    // non-movable: shared_mutex is non-movable. store keeps unique_ptr<version_chain>.
    version_chain(version_chain&&) = delete;
    version_chain& operator=(version_chain&&) = delete;

    // newest committed version visible at the given snapshot, or nullptr if
    // none. tombstone visibility is the caller's call.
    const version* visible_at(ts_t snapshot) const {
        std::shared_lock lk(mu_);
        const version* cur = head_.get();
        while (cur != nullptr && cur->begin_ts > snapshot) {
            cur = cur->next.get();
        }
        return cur;
    }

    // pushes a new committed version on the head. caller is txn_manager::commit.
    // commit_ts must be strictly greater than the current head's begin_ts.
    void install(std::unique_ptr<version> v) {
        std::unique_lock lk(mu_);
        if (head_ != nullptr) {
            head_->end_ts = v->begin_ts;
        }
        v->next = std::move(head_);
        head_ = std::move(v);
    }

    bool empty() const {
        std::shared_lock lk(mu_);
        return head_ == nullptr;
    }

    // true if any installed version has begin_ts > snapshot. used by commit's
    // FCW check to detect a concurrent committed writer of this key.
    bool has_newer_than(ts_t snapshot) const {
        std::shared_lock lk(mu_);
        return head_ != nullptr && head_->begin_ts > snapshot;
    }

private:
    mutable std::shared_mutex mu_;
    std::unique_ptr<version> head_;
};

} // namespace ssikv
