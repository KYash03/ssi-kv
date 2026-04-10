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

status txn_manager::write(transaction& t, const std::string& k, const std::string& v) {
    if (!t.active()) return status::err_txn_not_active;
    t.writes[k] = v;
    return status::ok;
}

status txn_manager::del(transaction& t, const std::string& k) {
    if (!t.active()) return status::err_txn_not_active;
    t.writes[k] = std::nullopt;
    return status::ok;
}

status txn_manager::read(transaction& t, const std::string& k, std::string& out) {
    if (!t.active()) return status::err_txn_not_active;

    // read-your-writes: the txn's own writes shadow the snapshot. checked
    // before recording in the read-set; the read-set is for FCW conflict
    // detection against other txns, not against ourselves.
    if (auto it = t.writes.find(k); it != t.writes.end()) {
        if (!it->second.has_value()) return status::not_found; // own tombstone
        out = *it->second;
        return status::ok;
    }

    t.reads.insert(k);

    const auto* chain = store_.find_chain(k);
    if (chain == nullptr) return status::not_found;

    const auto* v = chain->visible_at(t.start_ts);
    if (v == nullptr || v->tombstone) return status::not_found;

    out = v->value;
    return status::ok;
}

} // namespace ssikv
