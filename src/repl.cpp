#include "repl.h"

#include <cctype>
#include <sstream>

namespace ssikv {

namespace {

void trim(std::string_view& s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
}

// splits "VERB rest" into (verb, rest). rest keeps internal spaces (so PUT can
// take a value with spaces in the rest of the line).
std::pair<std::string_view, std::string_view> head_tail(std::string_view s) {
    auto sp = s.find(' ');
    if (sp == std::string_view::npos) return {s, std::string_view{}};
    return {s.substr(0, sp), s.substr(sp + 1)};
}

std::string ok() { return "OK"; }
std::string ok(std::string_view rest) {
    std::string r = "OK ";
    r.append(rest);
    return r;
}
std::string err(std::string_view what) {
    std::string r = "ERR ";
    r.append(what);
    return r;
}
std::string aborted(std::string_view what) {
    std::string r = "ABORT ";
    r.append(what);
    return r;
}

} // namespace

std::string handle_line(session& sess, txn_manager& tm, std::string_view line) {
    trim(line);
    if (line.empty()) return ok();

    auto [verb, rest] = head_tail(line);
    trim(rest);

    if (verb == "BEGIN") {
        if (sess.current != nullptr) return err("txn already active");
        sess.current = tm.begin();
        std::ostringstream os;
        os << "OK txn=" << sess.current->id << " start_ts=" << sess.current->start_ts;
        return os.str();
    }

    if (verb == "GET") {
        if (sess.current == nullptr) return err("no active txn");
        if (rest.empty()) return err("usage: GET <key>");
        std::string out;
        auto s = tm.read(*sess.current, std::string{rest}, out);
        if (s == status::ok) return ok(out);
        if (s == status::not_found) return ok("<nil>");
        return aborted(to_string(s));
    }

    if (verb == "PUT") {
        if (sess.current == nullptr) return err("no active txn");
        auto [k, v] = head_tail(rest);
        if (k.empty() || v.empty()) return err("usage: PUT <key> <value>");
        auto s = tm.write(*sess.current, std::string{k}, std::string{v});
        if (s == status::ok) return ok();
        return aborted(to_string(s));
    }

    if (verb == "DEL") {
        if (sess.current == nullptr) return err("no active txn");
        if (rest.empty()) return err("usage: DEL <key>");
        auto s = tm.del(*sess.current, std::string{rest});
        if (s == status::ok) return ok();
        return aborted(to_string(s));
    }

    if (verb == "COMMIT") {
        if (sess.current == nullptr) return err("no active txn");
        auto s = tm.commit(*sess.current);
        auto* t = sess.current;
        sess.current = nullptr;
        tm.gc_sireads();
        if (s == status::ok) {
            std::ostringstream os;
            os << "OK commit_ts=" << t->commit_ts;
            return os.str();
        }
        return aborted(to_string(s));
    }

    if (verb == "ROLLBACK") {
        if (sess.current == nullptr) return err("no active txn");
        tm.abort(*sess.current, "user");
        sess.current = nullptr;
        tm.gc_sireads();
        return ok();
    }

    if (verb == "QUIT") {
        if (sess.current != nullptr) {
            tm.abort(*sess.current, "client_disconnected");
            sess.current = nullptr;
        }
        return "BYE";
    }

    return err("unknown verb");
}

} // namespace ssikv
