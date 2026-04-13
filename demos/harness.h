#pragma once

// tiny demo helper. each demo binary is a `main()` that scripts a known
// anomaly schedule and prints PASS / FAIL based on whether the expected
// commits/aborts happened. exit 0 on PASS, 1 on FAIL.

#include <ssikv/status.h>

#include <iostream>
#include <string_view>

namespace ssikv::demo {

inline int report(bool pass, std::string_view name) {
    std::cout << (pass ? "PASS" : "FAIL") << "  " << name << "\n";
    return pass ? 0 : 1;
}

inline bool expect_eq(status got, status want, std::string_view what) {
    if (got != want) {
        std::cerr << "  " << what << ": got=" << to_string(got)
                  << " want=" << to_string(want) << "\n";
        return false;
    }
    return true;
}

} // namespace ssikv::demo
