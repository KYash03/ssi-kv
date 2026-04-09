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

status txn_manager::write(transaction& t, const key_t& k, const val_t& v) {
    if (!t.active()) return status::err_txn_not_active;
    t.writes[k] = v;
    return status::ok;
}

status txn_manager::del(transaction& t, const key_t& k) {
    if (!t.active()) return status::err_txn_not_active;
    t.writes[k] = std::nullopt;
    return status::ok;
}

} // namespace ssikv
