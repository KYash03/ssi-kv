#pragma once

#include <cstdint>
#include <limits>
#include <string>

namespace ssikv {

using txn_id_t = uint64_t;
using ts_t = uint64_t; // logical timestamp; monotonic counter, see txn_manager
using page_id_t = uint64_t;
using key_t = std::string;
using val_t = std::string;

constexpr txn_id_t kInvalidTxn = 0;
constexpr ts_t kTsInfinity = std::numeric_limits<ts_t>::max();

} // namespace ssikv
