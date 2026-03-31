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

each demo is a small program that scripts a known anomaly and prints PASS or FAIL.
binaries land in `build/demos/` as the project progresses.

## TODO

* persistence (wal + recovery)
* row-level / predicate siread locks (currently page-level)
* safe-snapshot read-only optimization (ports & grittner §4)
* summarization to bound memory (ports & grittner §6)
* multi-node replication
