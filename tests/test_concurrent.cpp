// concurrency stress: many threads racing through begin/read/write/commit.
// goal isn't to verify a specific anomaly outcome — that's the demos. goal
// here is to drive the contended code paths so TSAN has something to bite.
//
// invariants checked:
//   - committed txns sum to a reasonable count (no double-commit accounting).
//   - the chain head for a contended key is monotonically increasing in
//     commit_ts (no torn install).
//   - aborts only happen with abort_si_first_committer_wins or
//     aborted_ssi_dangerous_structure (no spurious err_*).

#include <catch_amalgamated.hpp>

#include <ssikv/store.h>
#include <ssikv/txn_manager.h>
#include <ssikv/version.h>
#include <ssikv/version_chain.h>

#include <atomic>
#include <random>
#include <thread>
#include <vector>

using namespace ssikv;

namespace {

struct counts {
    std::atomic<uint64_t> commits{0};
    std::atomic<uint64_t> fcw{0};
    std::atomic<uint64_t> ssi{0};
    std::atomic<uint64_t> other_aborts{0};
};

void hammer(txn_manager& tm, int iters, int n_keys, uint32_t seed, counts& c) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> pick(0, n_keys - 1);
    for (int i = 0; i < iters; ++i) {
        auto* t = tm.begin();
        std::string out;
        // small mixed read/write footprint
        const int a = pick(rng);
        const int b = pick(rng);
        (void)tm.read(*t, "k:" + std::to_string(a), out);
        (void)tm.read(*t, "k:" + std::to_string(b), out);
        (void)tm.write(*t, "k:" + std::to_string(a), "v" + std::to_string(i));
        switch (tm.commit(*t)) {
        case status::ok: c.commits.fetch_add(1); break;
        case status::aborted_si_first_committer_wins: c.fcw.fetch_add(1); break;
        case status::aborted_ssi_dangerous_structure: c.ssi.fetch_add(1); break;
        default: c.other_aborts.fetch_add(1); break;
        }
        if ((i & 0x3F) == 0) tm.gc_sireads();
    }
}

} // namespace

TEST_CASE("many threads, low contention", "[concurrent]") {
    store s;
    txn_manager tm(s);

    counts c;
    constexpr int THREADS = 8;
    constexpr int ITERS = 500;
    constexpr int N_KEYS = 256;

    std::vector<std::thread> ts;
    for (int i = 0; i < THREADS; ++i) {
        ts.emplace_back(hammer, std::ref(tm), ITERS, N_KEYS, uint32_t(i + 1),
                        std::ref(c));
    }
    for (auto& t : ts) t.join();

    CAPTURE(c.commits.load(), c.fcw.load(), c.ssi.load(), c.other_aborts.load());
    REQUIRE(c.other_aborts.load() == 0); // no err_* leaks
    REQUIRE(c.commits.load() + c.fcw.load() + c.ssi.load() ==
            uint64_t(THREADS) * uint64_t(ITERS));
}

TEST_CASE("many threads, high contention drives ssi aborts", "[concurrent]") {
    store s;
    txn_manager tm(s);

    counts c;
    constexpr int THREADS = 8;
    constexpr int ITERS = 500;
    constexpr int N_KEYS = 4; // tight contention

    std::vector<std::thread> ts;
    for (int i = 0; i < THREADS; ++i) {
        ts.emplace_back(hammer, std::ref(tm), ITERS, N_KEYS, uint32_t(i + 1),
                        std::ref(c));
    }
    for (auto& t : ts) t.join();

    REQUIRE(c.other_aborts.load() == 0);
    // we expect at least some aborts at this contention level. if not, the
    // detection paths are silent and that's a bug.
    REQUIRE(c.fcw.load() + c.ssi.load() > 0);
}

TEST_CASE("chain heads under concurrent writers stay monotonic", "[concurrent][chain]") {
    store s;
    txn_manager tm(s);

    constexpr int THREADS = 8;
    constexpr int ITERS = 200;

    std::vector<std::thread> ts;
    for (int i = 0; i < THREADS; ++i) {
        ts.emplace_back([&tm, i] {
            for (int n = 0; n < ITERS; ++n) {
                auto* t = tm.begin();
                (void)tm.write(*t, "hot", "v" + std::to_string(i) + "-" +
                                              std::to_string(n));
                (void)tm.commit(*t);
            }
        });
    }
    for (auto& t : ts) t.join();

    const auto* chain = s.find_chain("hot");
    REQUIRE(chain != nullptr);

    // walk back from head: begin_ts must be strictly decreasing.
    ts_t prev = kTsInfinity;
    chain->for_each_newer(0, [&](const version& v) {
        REQUIRE(v.begin_ts < prev);
        prev = v.begin_ts;
    });
}
