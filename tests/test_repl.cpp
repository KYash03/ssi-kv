#include <catch_amalgamated.hpp>

#include <ssikv/repl.h>

#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

using namespace ssikv;

TEST_CASE("BEGIN/PUT/COMMIT round trip", "[repl]") {
    store s;
    txn_manager tm(s);
    session sess;

    auto r = handle_line(sess, tm, "BEGIN");
    REQUIRE(r.starts_with("OK txn="));
    REQUIRE(handle_line(sess, tm, "PUT k v").starts_with("OK"));
    REQUIRE(handle_line(sess, tm, "COMMIT").starts_with("OK commit_ts="));
}

TEST_CASE("GET on uncommitted snapshot sees nil for missing key", "[repl]") {
    store s;
    txn_manager tm(s);
    session sess;
    REQUIRE(handle_line(sess, tm, "BEGIN").starts_with("OK"));
    REQUIRE(handle_line(sess, tm, "GET missing") == "OK <nil>");
}

TEST_CASE("ROLLBACK ends txn cleanly", "[repl]") {
    store s;
    txn_manager tm(s);
    session sess;
    REQUIRE(handle_line(sess, tm, "BEGIN").starts_with("OK"));
    REQUIRE(handle_line(sess, tm, "ROLLBACK") == "OK");
    REQUIRE(handle_line(sess, tm, "COMMIT") == "ERR no active txn");
}

TEST_CASE("unknown verb yields ERR", "[repl]") {
    store s;
    txn_manager tm(s);
    session sess;
    REQUIRE(handle_line(sess, tm, "BLERG").starts_with("ERR"));
}
