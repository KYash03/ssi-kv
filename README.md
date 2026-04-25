# ssi-kv

in-memory mvcc key-value store with serializable snapshot isolation.
single node, no persistence.

based on:
- berenson et al. 1995, *a critique of ansi sql isolation levels*, sigmod
  (https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/tr-95-51.pdf)
- adya, liskov, o'neil 2000, *generalized isolation level definitions*, icde
  (https://pmg.csail.mit.edu/pubs/adya00generalized-abstract.html)
- fekete et al. 2005, *making snapshot isolation serializable*, tods
  (https://dsf.berkeley.edu/cs286/papers/ssi-tods2005.pdf)
- cahill, röhm, fekete 2008, *serializable isolation for snapshot databases*, sigmod
  (https://people.eecs.berkeley.edu/~kubitron/courses/cs262a-F13/handouts/papers/p729-cahill.pdf)
- ports & grittner 2012, *serializable snapshot isolation in postgresql*, vldb
  (https://www.drkp.net/papers/ssi-vldb12.pdf)

## running

    cmake -B build
    cmake --build build -j
    ./build/apps/ssikv

## tests

    ctest --test-dir build --output-on-failure

## demos

each demo scripts a known anomaly and prints PASS or FAIL. run them all with

    for d in build/demos/demo_*; do "$d" || echo "FAIL: $d"; done

what's in there:

* `demo_lost_update`        - berenson 1995 P4. SI's first-committer-wins.
* `demo_write_skew`         - berenson 1995 A5B. SSI catches it.
* `demo_doctors_on_call`    - cahill 2008 fig 1.
* `demo_read_only_anomaly`  - fekete 2005 ex 1.3. read-only txn in the cycle.
* `demo_joint_balance`      - smallbank-flavoured write skew. see demo source
                              for why it's not the literal cahill 2008 §5.1
                              pivot (that one needs a 3-txn schedule and
                              schema choices that don't fit a simple demo).
* `demo_tpcc_subset`        - fekete 2005 §4. must NOT abort. false-positive
                              sanity check.

## sanitizers

asan/ubsan are on by default in Debug builds. for tsan:

    ./scripts/run_sanitizers.sh

## docs

* [docs/invariants.md](docs/invariants.md) - lock ordering, ts ordering,
  rw-edge insertion sites, why cahill's commit-ordering optimisation is a
  no-op in this tracking model.

## TODO

* persistence (wal + recovery)
* row-level / predicate siread locks (currently page-level)
* safe-snapshot read-only optimization (ports & grittner §4)
* summarization to bound memory (ports & grittner §6)
* multi-node replication
