# invariants

things that must hold for correctness. each line below has a paper reference
or a structural reason. if you change code, run it past this list.

## timestamp ordering

* `start_ts` and `commit_ts` come from one seq_cst counter (`txn_manager::ts_`).
  any commit_ts is therefore strictly greater than any start_ts that was read
  before it. this is what cahill 2008 §3.2 requires.
* a committed version's `begin_ts` equals its creator's `commit_ts`. installed
  versions are temporally ordered by commit_ts in the chain (head = newest).
* `commit_ts` is assigned once, in `txn_manager::commit`, after FCW and the
  dangerous-structure check pass. installs reuse it for every key.

## lock ordering

acquired top-down. only `commit()` touches more than one of these in the same
call.

```
store::mu_              (insert chain into chains_ map)
  version_chain::mu_    (read or install head)
    lock_manager::mu_   (write locks; FCW)
      siread_lock_manager::mu_  (siread acquire / on_finish / gc / holders_of)
      txn_manager::graph_mu_    (in/out_conflicts mutation)
```

`siread` and `graph` are leaves; either one can be taken without the other.
when `commit()` does both, `siread` first (to query holders), then `graph`
(to add edges), so they're effectively a sequence, not nested.

`txn_manager::active_mu_` is taken alone for active-map mutation and read.
do not nest any of the above inside it; do not nest it inside `graph_mu_`.

## rw-antidependency edges: where they come from

every rw-edge is created at exactly one of these two points. anywhere else is
a bug.

1. **read of a newer version** (`txn_manager::read`, after `for_each_newer`):
   reader walks past versions whose `begin_ts > t.start_ts`. each such version
   was installed by a committed concurrent writer. add edge reader -> writer.

2. **write to a siread-held page** (`txn_manager::commit`, before assigning
   commit_ts): for every key in the write-set, query siread holders of the
   key's page. each holder is a reader that took its snapshot before our
   commit. add edge holder -> us.

both calls take `graph_mu_` while mutating `in_conflicts` / `out_conflicts`.

## state ownership

* `transaction::st` is `std::atomic<state>`. the owning thread writes it from
  `active` to `committed`/`aborted`; other threads read it via `active()`
  while holding `active_mu_` (e.g. `gc_sireads`). the atomic is what makes
  that read-vs-write safe; nothing else protects it.
* the rest of `transaction` (`writes`, `reads`, `sireads`, `commit_ts`,
  `start_ts`, `id`, `abort_reason`) is written only by the owning thread.
  other threads read these in narrow contexts that are quiescent w.r.t. the
  owner (e.g. reading start_ts for gc, after the owning thread is past
  `begin()` and the value is fixed).
* `in_conflicts` / `out_conflicts` are written only under `graph_mu_`. reads
  of them in `commit()` are also under `graph_mu_`.

## version chain ownership

* a `version_chain` owns its versions via `unique_ptr` linked list. the only
  writer is `install()` under exclusive `mu_`.
* readers walk the chain holding shared `mu_`. while shared is held, no
  install can free under them. after they release, the chain's structure may
  change but the versions they observed are still pinned by the chain.
* a `version_chain` is non-movable (its `shared_mutex` is). `store` keeps
  `unique_ptr<version_chain>` so the map can grow.

## fcw atomicity

`commit()` does:

```
acquire wlocks_ on all write-set keys     (fails -> abort_si_first_committer_wins)
for each write-set key, check chain.has_newer_than(start_ts)
                                          (fails -> abort_si_first_committer_wins)
... ssi check ...
assign commit_ts
install versions into chains
release wlocks_
release siread locks via on_finish
mark txn committed
```

the wlock acquisition is the serialization point that makes the
"check-then-install" pair atomic against other commits on the same keys.
without it two concurrent commits could each see a clean chain head and both
install — which would not violate FCW per se (since chains accept any number
of installs) but would violate the invariant that no two concurrent committed
txns wrote the same key.

## ssi false-positive note (cahill thesis / ports & grittner §3.3.1)

cahill proposes a commit-ordering optimization: even if a dangerous structure
{T1 -rw-> T2 -rw-> T3} is found, no abort is needed if T1 or T2 commits
before T3.

**this optimization is a no-op in our impl.** out_conflict edges are added
only at the read-side (path 1 above), where the writer is already in the
chain — so it has already committed. by the time T2 (us) is in `commit()`
considering `out_conflicts`, every member there is already committed. T3 is
always "first to commit" relative to T2. so the rule "abort iff some
out_conflict has committed" reduces to "abort iff out_conflicts non-empty",
which is what we already check.

a richer tracking model (ports & grittner do this with mvcc visibility data
in §5.2) can record edges to still-active out-partners, where the
optimization fires. that's a v2-or-later refinement. v1's false-positive
rate is already as tight as this tracking shape allows.

## what aborts the pivot vs. one of its neighbours

we always abort the txn whose `commit()` triggers the detection. for the
canonical write-skew shape that's the pivot (T2). for the read-only-anomaly
shape (fekete 2005 ex 1.3) it's also the pivot (T2 the writer, not T3 the
read-only). this is the safe-retry property of ports & grittner §5.4: when
the pivot retries, T_in and T_out are now committed in the past relative to
the new snapshot, so the same dangerous structure cannot recur with the same
neighbours.

## things i deliberately left as TODO

* `active_` map cleanup. terminated txn records linger forever in v1. fine
  for short demos; eventually a long-lived process needs reclamation.
* siread granularity is page-level. row-level + index-range siread (next-key
  locking) is a v2 feature; v1 sidesteps phantoms by being coarse.
* persistence. nothing on disk; restart loses everything.
* read-only safe-snapshot optimization (ports & grittner §4). a read-only
  txn in v1 still takes siread locks just like an update txn.
