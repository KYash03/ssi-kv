// throughput + abort rate under varying conflict rates. csv on stdout:
//   contention,threads,duration_ms,commits,aborts,fcw_aborts,ssi_aborts,tps
//
// each thread runs a tight read-modify-write loop on N keys. higher contention
// (smaller N) -> more rw-conflicts -> more ssi aborts.

#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using namespace ssikv;

struct counts {
    std::atomic<uint64_t> commits{0};
    std::atomic<uint64_t> fcw{0};
    std::atomic<uint64_t> ssi{0};
};

static void worker(txn_manager& tm, int n_keys, std::chrono::milliseconds duration,
                   counts& c, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> pick(0, n_keys - 1);
    auto end = std::chrono::steady_clock::now() + duration;

    while (std::chrono::steady_clock::now() < end) {
        auto* t = tm.begin();
        std::string out;
        // read two random keys, write one of them.
        int a = pick(rng), b = pick(rng);
        (void)tm.read(*t, "k:" + std::to_string(a), out);
        (void)tm.read(*t, "k:" + std::to_string(b), out);
        (void)tm.write(*t, "k:" + std::to_string(a), "v");
        auto s = tm.commit(*t);
        switch (s) {
        case status::ok: c.commits.fetch_add(1); break;
        case status::aborted_si_first_committer_wins: c.fcw.fetch_add(1); break;
        case status::aborted_ssi_dangerous_structure: c.ssi.fetch_add(1); break;
        default: break;
        }
        // periodic gc
        if ((c.commits.load() & 0xFF) == 0) tm.gc_sireads();
    }
}

int main(int argc, char** argv) {
    int threads = 4;
    int duration_ms = 1000;
    if (argc >= 2) threads = std::atoi(argv[1]);
    if (argc >= 3) duration_ms = std::atoi(argv[2]);

    std::cout << "contention,threads,duration_ms,commits,aborts,fcw,ssi,tps\n";

    for (int n_keys : {2, 8, 32, 128, 1024}) {
        store s;
        txn_manager tm(s);
        // seed.
        {
            auto* t = tm.begin();
            for (int i = 0; i < n_keys; ++i) {
                (void)tm.write(*t, "k:" + std::to_string(i), "0");
            }
            (void)tm.commit(*t);
        }

        counts c;
        std::vector<std::thread> ts;
        for (int i = 0; i < threads; ++i) {
            ts.emplace_back(worker, std::ref(tm), n_keys,
                            std::chrono::milliseconds(duration_ms), std::ref(c),
                            uint32_t(i + 1));
        }
        for (auto& t : ts) t.join();

        uint64_t commits = c.commits.load();
        uint64_t fcw = c.fcw.load();
        uint64_t ssi = c.ssi.load();
        uint64_t aborts = fcw + ssi;
        double tps = double(commits) * 1000.0 / double(duration_ms);

        std::cout << n_keys << "," << threads << "," << duration_ms << "," << commits
                  << "," << aborts << "," << fcw << "," << ssi << "," << tps << "\n";
    }
    return 0;
}
