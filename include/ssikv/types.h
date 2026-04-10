#pragma once

#include <cstdint>
#include <limits>

namespace ssikv {

// posix already defines key_t in <sys/types.h>; we use plain std::string for
// keys/values throughout instead of typedefs to dodge the collision.
using txn_id_t = uint64_t;
using ts_t = uint64_t; // logical timestamp; monotonic counter, see txn_manager
using page_id_t = uint64_t;

constexpr txn_id_t kInvalidTxn = 0;
constexpr ts_t kTsInfinity = std::numeric_limits<ts_t>::max();

} // namespace ssikv
