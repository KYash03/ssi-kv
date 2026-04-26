// minimal tcp server: blocking accept loop, one std::jthread per connection.
// no framing beyond newlines. fine for v1; production would use non-blocking
// IO and a thread pool.

#include <ssikv/repl.h>
#include <ssikv/txn_manager.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace ssikv {

static void serve(int fd, txn_manager& tm) {
    session sess;
    std::string buf;
    char chunk[1024];

    auto send_line = [&](const std::string& s) {
        std::string out = s;
        out.push_back('\n');
        ::send(fd, out.data(), out.size(), 0);
    };

    while (true) {
        ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0) break;
        buf.append(chunk, static_cast<size_t>(n));

        // process complete lines
        while (true) {
            auto nl = buf.find('\n');
            if (nl == std::string::npos) break;
            std::string line = buf.substr(0, nl);
            buf.erase(0, nl + 1);

            std::string reply = handle_line(sess, tm, line);
            send_line(reply);
            if (reply == "BYE") {
                ::close(fd);
                return;
            }
        }
    }
    if (sess.current != nullptr) {
        tm.abort(*sess.current, "client_disconnected");
        tm.gc_sireads();
    }
    ::close(fd);
}

} // namespace ssikv

int run_tcp(ssikv::txn_manager& tm, int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket: " << std::strerror(errno) << "\n";
        return 1;
    }
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "bind: " << std::strerror(errno) << "\n";
        return 1;
    }
    if (::listen(fd, 16) != 0) {
        std::cerr << "listen: " << std::strerror(errno) << "\n";
        return 1;
    }
    std::cerr << "ssikv listening on 127.0.0.1:" << port << "\n";

    // detach per-connection threads. infinite accept loop, no orderly
    // shutdown in v1; jthread would be cleaner but isn't available on the
    // older libc++ shipped on macos ci runners.
    while (true) {
        int conn = ::accept(fd, nullptr, nullptr);
        if (conn < 0) {
            if (errno == EINTR) continue;
            std::cerr << "accept: " << std::strerror(errno) << "\n";
            break;
        }
        std::thread([conn, &tm] { ssikv::serve(conn, tm); }).detach();
    }
    ::close(fd);
    return 0;
}
