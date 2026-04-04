#include <ssikv/store.h>

namespace ssikv {

version_chain& store::chain_for(const key_t& k) {
    {
        std::shared_lock lk(mu_);
        auto it = chains_.find(k);
        if (it != chains_.end()) {
            return *it->second;
        }
    }
    std::unique_lock lk(mu_);
    auto [it, inserted] = chains_.try_emplace(k, std::make_unique<version_chain>());
    return *it->second;
}

const version_chain* store::find_chain(const key_t& k) const {
    std::shared_lock lk(mu_);
    auto it = chains_.find(k);
    return it == chains_.end() ? nullptr : it->second.get();
}

} // namespace ssikv
