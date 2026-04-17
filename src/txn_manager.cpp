#include <ssikv/txn_manager.h>
#include <ssikv/version.h>

#include <vector>

namespace ssikv {

void txn_manager::add_rw_edge(transaction& reader, transaction& writer) {
    if (reader.id == writer.id) return;
    reader.out_conflicts.insert(writer.id);
    writer.in_conflicts.insert(reader.id);
}

transaction* txn_manager::find_active(txn_id_t id) {
    std::lock_guard lk(active_mu_);
    auto it = active_.find(id);
    if (it == active_.end()) return nullptr;
    return it->second->active() ? it->second.get() : nullptr;
}

transaction* txn_manager::find_known(txn_id_t id) {
    std::lock_guard lk(active_mu_);
    auto it = active_.find(id);
    return it == active_.end() ? nullptr : it->second.get();
}

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

void txn_manager::abort(transaction& t, std::string_view reason) {
    if (!t.active()) return;
    t.st = transaction::state::aborted;
    t.abort_reason.assign(reason);
    wlocks_.release_all(t.id);
    // siread locks live past abort just like past commit (cahill 2008 §3.4):
    // a write that lands later still needs to see them. on_finish marks them
    // collectable; gc reclaims when the oldest active txn moves past finish_ts.
    sirlocks_.on_finish(t.id, ts_.load(std::memory_order_seq_cst));
}

status txn_manager::commit(transaction& t) {
    if (!t.active()) return status::err_txn_not_active;

    // gather write-set keys.
    std::vector<std::string> keys;
    keys.reserve(t.writes.size());
    for (const auto& [k, _] : t.writes) {
        keys.push_back(k);
    }

    // lock_manager acquisition serializes commits against each other on the
    // same keys. without this, two concurrent commits could both pass the
    // chain-head check and install conflicting versions.
    if (!keys.empty() && !wlocks_.try_acquire_all(t.id, keys)) {
        abort(t, "aborted_si_first_committer_wins");
        return status::aborted_si_first_committer_wins;
    }

    // FCW: any of our keys has a committed version newer than our snapshot ->
    // a concurrent committed writer beat us (berenson 1995 §4.2).
    for (const auto& k : keys) {
        if (auto* chain = store_.find_chain(k); chain && chain->has_newer_than(t.start_ts)) {
            abort(t, "aborted_si_first_committer_wins");
            return status::aborted_si_first_committer_wins;
        }
    }

    // assign commit_ts FIRST so every installed version stamps the same ts.
    // ordering: commit_ts > start_ts of any concurrent committed txn we
    // serialize after (cahill 2008 §3.2).
    t.commit_ts = next_ts();

    for (auto& [k, v] : t.writes) {
        auto& chain = store_.chain_for(k);
        const bool dead = !v.has_value();
        chain.install(std::make_unique<version>(
            t.commit_ts, dead ? std::string{} : *v, dead, t.id));
    }

    t.st = transaction::state::committed;
    wlocks_.release_all(t.id);
    sirlocks_.on_finish(t.id, t.commit_ts);
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
    const page_id_t page = store_.page_for(k);
    sirlocks_.acquire(t.id, page);
    t.sireads.insert(page);

    const auto* chain = store_.find_chain(k);
    if (chain == nullptr) return status::not_found;

    // read-side rw-antidep: for every committed version newer than our
    // snapshot, the writer is concurrent with us. record reader -> writer.
    chain->for_each_newer(t.start_ts, [&](const version& nv) {
        if (auto* w = find_known(nv.creator); w != nullptr && w->id != t.id) {
            std::lock_guard lk(graph_mu_);
            add_rw_edge(t, *w);
        }
    });

    const auto* v = chain->visible_at(t.start_ts);
    if (v == nullptr || v->tombstone) return status::not_found;

    out = v->value;
    return status::ok;
}

} // namespace ssikv
