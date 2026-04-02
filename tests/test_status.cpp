#include <catch_amalgamated.hpp>
#include <ssikv/status.h>

using namespace ssikv;

TEST_CASE("status round-trips through to_string", "[status]") {
    REQUIRE(to_string(status::ok) == "ok");
    REQUIRE(to_string(status::aborted_ssi_dangerous_structure) ==
            "aborted_ssi_dangerous_structure");
}

TEST_CASE("format_abort_reason joins detail", "[status]") {
    auto s = format_abort_reason(status::aborted_ssi_dangerous_structure,
                                 "pivot=T7 in=[T3] out=[T9]");
    REQUIRE(s == "aborted_ssi_dangerous_structure pivot=T7 in=[T3] out=[T9]");
}
