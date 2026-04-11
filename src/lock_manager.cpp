#include <ssikv/lock_manager.h>

namespace ssikv {

bool lock_manager::try_acquire_all(txn_id_t txn, const std::vector<std::string>& keys) {
    std::lock_guard lk(mu_);
    // first pass: every key must be either free or already held by us.
    for (const auto& k : keys) {
        auto it = holders_.find(k);
        if (it != holders_.end() && it->second != txn) {
            return false;
        }
    }
    // second pass: actually claim. safe because we hold mu_ across both passes.
    for (const auto& k : keys) {
        holders_[k] = txn;
    }
    return true;
}

void lock_manager::release_all(txn_id_t txn) {
    std::lock_guard lk(mu_);
    for (auto it = holders_.begin(); it != holders_.end();) {
        if (it->second == txn) {
            it = holders_.erase(it);
        } else {
            ++it;
        }
    }
}

std::optional<txn_id_t> lock_manager::holder(const std::string& k) const {
    std::lock_guard lk(mu_);
    auto it = holders_.find(k);
    if (it == holders_.end()) return std::nullopt;
    return it->second;
}

} // namespace ssikv
