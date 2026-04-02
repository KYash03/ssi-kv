#pragma once

#include <string>
#include <string_view>

namespace ssikv {

// outcomes of a txn op. ok means the call succeeded; aborted_* means the txn
// is dead and the caller should stop using it.
enum class status {
    ok,
    not_found,
    aborted_si_first_committer_wins, // ww-conflict at commit, berenson 1995 §4.2
    aborted_ssi_dangerous_structure, // pivot has both rw-in and rw-out, fekete 2005 thm 2.1
    aborted_user,                    // explicit ROLLBACK
    err_txn_not_active,              // op called on committed/aborted/unknown txn
    err_protocol,                    // malformed request from frontend
};

std::string_view to_string(status s);

// human-friendly abort reason for the wire/repl protocol. demos and the wire
// frontend both surface this verbatim.
std::string format_abort_reason(status s, std::string_view detail = {});

} // namespace ssikv
