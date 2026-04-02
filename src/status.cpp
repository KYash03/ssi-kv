#include <ssikv/status.h>

#include <string>

namespace ssikv {

std::string_view to_string(status s) {
    switch (s) {
    case status::ok:
        return "ok";
    case status::not_found:
        return "not_found";
    case status::aborted_si_first_committer_wins:
        return "aborted_si_first_committer_wins";
    case status::aborted_ssi_dangerous_structure:
        return "aborted_ssi_dangerous_structure";
    case status::aborted_user:
        return "aborted_user";
    case status::err_txn_not_active:
        return "err_txn_not_active";
    case status::err_protocol:
        return "err_protocol";
    }
    return "unknown";
}

std::string format_abort_reason(status s, std::string_view detail) {
    std::string out{to_string(s)};
    if (!detail.empty()) {
        out.push_back(' ');
        out.append(detail);
    }
    return out;
}

} // namespace ssikv
