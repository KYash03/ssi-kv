#include <ssikv/txn_manager.h>

namespace ssikv {

transaction* txn_manager::begin() {
    auto t = std::make_unique<transaction>();
    t->start_ts = next_ts();
    {
        std::lock_guard lk(active_mu_);
        t->id = next_id_++;
        auto* raw = t.get();
        active_.emplace(t->id, std::move(t));
        return raw;
    }
}

} // namespace ssikv
