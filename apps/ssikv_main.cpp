// ssikv server entrypoint. supports either a stdin repl (--stdin) or a tcp
// listener (--port N). default is --stdin so `./ssikv` works without args.

#include <ssikv/repl.h>
#include <ssikv/store.h>
#include <ssikv/txn_manager.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

using namespace ssikv;

static int run_stdin(txn_manager& tm) {
    session sess;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::string reply = handle_line(sess, tm, line);
        std::cout << reply << "\n";
        std::cout.flush();
        if (reply == "BYE") break;
    }
    return 0;
}

int run_tcp(txn_manager& tm, int port); // commit 29

int main(int argc, char** argv) {
    int port = 0;
    bool stdin_mode = true;
    for (int i = 1; i < argc; ++i) {
        std::string_view a{argv[i]};
        if (a == "--stdin") {
            stdin_mode = true;
        } else if (a.starts_with("--port=")) {
            port = std::atoi(argv[i] + 7);
            stdin_mode = false;
        } else if (a == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
            stdin_mode = false;
        } else {
            std::cerr << "ssikv: unknown arg " << a << "\n";
            return 2;
        }
    }

    store s;
    txn_manager tm(s);
    if (stdin_mode) return run_stdin(tm);
    return run_tcp(tm, port);
}
