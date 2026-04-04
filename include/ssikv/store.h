#pragma once

#include <ssikv/types.h>
#include <ssikv/version_chain.h>

#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace ssikv {

// the in-memory keyspace. owns a version_chain per key, lazily created on first
// touch. page assignment is a stable hash of the key, used by the siread lock
// manager (cahill 2008 §3.4 - we lock at page granularity in v1).
class store {
public:
    static constexpr page_id_t kNumPages = 256;

    explicit store() = default;
    store(const store&) = delete;
    store& operator=(const store&) = delete;

    // returns the chain for the key, inserting an empty one if needed.
    version_chain& chain_for(const key_t& k);

    // read-only lookup. nullptr if key has never been written.
    const version_chain* find_chain(const key_t& k) const;

    page_id_t page_for(const key_t& k) const {
        // simple, deterministic; std::hash is fine for non-adversarial input.
        return std::hash<key_t>{}(k) % kNumPages;
    }

private:
    mutable std::shared_mutex mu_;
    std::unordered_map<key_t, std::unique_ptr<version_chain>> chains_;
};

} // namespace ssikv
