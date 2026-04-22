#pragma once

#include <ssikv/transaction.h>
#include <ssikv/txn_manager.h>

#include <string>
#include <string_view>

namespace ssikv {

// per-connection state. one txn at a time per session.
struct session {
    transaction* current = nullptr;
};

// dispatches a single newline-terminated line. returns the reply line (without
// trailing newline; the caller adds it). pure function on session +
// txn_manager so it's easy to test.
std::string handle_line(session& sess, txn_manager& tm, std::string_view line);

} // namespace ssikv
