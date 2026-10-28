// book: flags=-pthread
// Showdown, naive side (Section 15.6): the receiver most people write first. Blocking
// socket with the default receive buffer, bound to INADDR_ANY, one recvfrom() per
// datagram, each message copied out and dispatched through std::function. tsan-clean.
#include <string>
#include "showdown_harness.hpp"

void receive(Harness& h) {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    const sockaddr_in any = mc::addr("0.0.0.0", Harness::kPort);
    bind(fd, reinterpret_cast<const sockaddr*>(&any), sizeof any);
    ip_mreq m{};
    inet_pton(AF_INET, Harness::kGroup, &m.imr_multiaddr);
    inet_pton(AF_INET, "127.0.0.1", &m.imr_interface);
    setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof m);
    timeval tv{0, 100'000};                           // wake up to check for the end
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    std::function<void(std::uint64_t, const std::string&)> handler =
        [&h](std::uint64_t seq, const std::string& msg) {
            h.on_message(seq, std::as_bytes(std::span(msg.data(), msg.size())));
        };
    h.ready.store(true, std::memory_order_release);
    char buf[2048];
    while (!h.done.load(std::memory_order_acquire)) {
        const ssize_t n = recvfrom(fd, buf, sizeof buf, 0, nullptr, nullptr);   // sleeps
        if (n <= 0) continue;
        const auto v = feed::parse({reinterpret_cast<std::byte*>(buf), std::size_t(n)});
        if (!v) continue;
        feed::for_each_message(*v, 0, [&](std::uint64_t seq, auto payload) {
            handler(seq, std::string(reinterpret_cast<const char*>(payload.data()),
                                     payload.size()));
        });
    }
    close(fd);
}

int main() {
    Harness h;
    h.run("naive receiver", receive);
}
