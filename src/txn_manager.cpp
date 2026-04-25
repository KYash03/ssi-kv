#include <ssikv/txn_manager.h>
#include <ssikv/version.h>

#include <vector>

namespace ssikv {

void txn_manager::add_rw_edge(transaction& reader, transaction& writer) {
    if (reader.id == writer.id) return;
    reader.out_conflicts.insert(writer.id);
    writer.in_conflicts.insert(reader.id);
}

std::shared_ptr<transaction> txn_manager::find_active(txn_id_t id) {
    std::lock_guard lk(active_mu_);
    auto it = active_.find(id);
    if (it == active_.end()) return nullptr;
    return it->second->active() ? it->second : nullptr;
}

size_t txn_manager::tracked_count() const {
    std::lock_guard lk(active_mu_);
    return active_.size();
}

std::shared_ptr<transaction> txn_manager::find_known(txn_id_t id) {
    std::lock_guard lk(active_mu_);
    auto it = active_.find(id);
    return it == active_.end() ? nullptr : it->second;
}

void txn_manager::gc_sireads() {
    // oldest active start_ts: scan the txn map. could be cached; not a
    // bottleneck at this scale.
    ts_t oldest = ts_.load(std::memory_order_seq_cst);
    {
        std::lock_guard lk(active_mu_);
        for (const auto& [_, tp] : active_) {
            if (tp->active() && tp->start_ts < oldest) oldest = tp->start_ts;
        }
    }
    sirlocks_.gc(oldest);

    // also drop terminated txn records whose finish_ts is in the strict past.
    // safe because:
    //   - read-side rw-edges only fire on versions newer than reader.start_ts,
    //     and oldest active reader has start_ts >= oldest > finish_ts, so no
    //     active reader can ever target this txn's installed versions.
    //   - write-side rw-edges fire from siread holders, which we just gc'd.
    //   - find_known() returning nullptr is a no-op in both edge sites.
    std::lock_guard lk(active_mu_);
    for (auto it = active_.begin(); it != active_.end();) {
        const auto& tp = it->second;
        if (!tp->active() && tp->finish_ts < oldest) {
            it = active_.erase(it);
        } else {
            ++it;
        }
    }
}

transaction* txn_manager::begin() {
    auto t = std::make_shared<transaction>();
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
    t.abort_reason.assign(reason);
    wlocks_.release_all(t.id);
    // siread locks live past abort just like past commit (cahill 2008 §3.4):
    // a write that lands later still needs to see them. on_finish marks them
    // collectable; gc reclaims when the oldest active txn moves past finish_ts.
    const ts_t now = ts_.load(std::memory_order_seq_cst);
    sirlocks_.on_finish(t.id, now);
    t.finish_ts = now;
    t.store_state(transaction::state::aborted);
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

    // write-side rw-antidep: any txn holding a SIREAD on a page we're about
    // to write into is a reader-with-an-older-snapshot. record holder -> us.
    // walked BEFORE we mark ourselves committed so concurrent reads still see
    // the right state.
    for (const auto& [k, _] : t.writes) {
        const page_id_t page = store_.page_for(k);
        for (txn_id_t holder_id : sirlocks_.holders_of(page)) {
            if (holder_id == t.id) continue;
            if (auto r = find_known(holder_id); r) {
                std::lock_guard lk(graph_mu_);
                add_rw_edge(*r, t);
            }
        }
    }

    // dangerous-structure check (fekete 2005 thm 2.1, cahill 2008 §3.1):
    // a non-serializable cycle requires two consecutive rw-edges between
    // concurrent txns. if we have both an incoming AND an outgoing rw-edge,
    // we are potentially the pivot of such a cycle. abort us to break it.
    // false positives are possible (we might not actually be in a cycle) but
    // the algorithm is conservative by design.
    {
        std::lock_guard lk(graph_mu_);
        if (!t.in_conflicts.empty() && !t.out_conflicts.empty()) {
            std::string detail = "in=[";
            bool first = true;
            for (txn_id_t id : t.in_conflicts) {
                if (!first) detail += ',';
                detail += 'T' + std::to_string(id);
                first = false;
            }
            detail += "] out=[";
            first = true;
            for (txn_id_t id : t.out_conflicts) {
                if (!first) detail += ',';
                detail += 'T' + std::to_string(id);
                first = false;
            }
            detail += ']';
            t.commit_ts = 0;
            abort(t, "aborted_ssi_dangerous_structure " + detail);
            return status::aborted_ssi_dangerous_structure;
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

    wlocks_.release_all(t.id);
    sirlocks_.on_finish(t.id, t.commit_ts);
    t.finish_ts = t.commit_ts;
    t.store_state(transaction::state::committed);
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
        if (auto w = find_known(nv.creator); w && w->id != t.id) {
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
