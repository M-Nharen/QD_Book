// book: norun
// Kernel-bypass receive with AF_XDP (Section 15.4). NOT RUN BY THE BUILD: it needs root
// (CAP_NET_ADMIN, CAP_BPF), takes over every packet arriving on one queue of a real
// interface, and only shows its point with a NIC whose driver supports zero-copy XDP.
// Usage: af_xdp_rx <ifname> <queue> <seconds>. No libbpf: raw bpf() syscalls throughout.
#include <linux/bpf.h>
#include <linux/if_link.h>
#include <linux/if_xdp.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include "feed_arbiter.hpp"

#ifndef AF_XDP
#define AF_XDP 44
#endif
#ifndef SOL_XDP
#define SOL_XDP 283
#endif

static void die(const char* what) { std::perror(what); std::exit(1); }
static long bpf(int cmd, bpf_attr& a) { return syscall(SYS_bpf, cmd, &a, sizeof a); }

constexpr std::uint32_t kFrames = 4096, kFrame = 2048, kRing = 2048;

struct Ring {                          // one of the four shared rings (cf. Listing 14.4)
    std::uint32_t* producer;
    std::uint32_t* consumer;
    std::uint32_t* flags;
    void* desc;
};

static Ring map_ring(int fd, const xdp_ring_offset& o, std::size_t elem, off_t pgoff) {
    const std::size_t len = o.desc + kRing * elem;
    auto* base = static_cast<char*>(mmap(nullptr, len, PROT_READ | PROT_WRITE,
                                         MAP_SHARED | MAP_POPULATE, fd, pgoff));
    if (base == MAP_FAILED) die("mmap ring");
    return {reinterpret_cast<std::uint32_t*>(base + o.producer),
            reinterpret_cast<std::uint32_t*>(base + o.consumer),
            reinterpret_cast<std::uint32_t*>(base + o.flags), base + o.desc};
}

// XDP program: return bpf_redirect_map(&xsks, ctx->rx_queue_index, XDP_PASS), i.e. hand
// every packet on a queue that has a socket to that socket; pass the rest to the kernel.
static int load_program(int map_fd) {
    bpf_insn prog[] = {
        {BPF_LDX | BPF_MEM | BPF_W, BPF_REG_2, BPF_REG_1,
         offsetof(xdp_md, rx_queue_index), 0},                 // r2 = queue index
        {BPF_LD | BPF_DW | BPF_IMM, BPF_REG_1, BPF_PSEUDO_MAP_FD, 0, map_fd},
        {0, 0, 0, 0, 0},                                       // (64-bit immediate)
        {BPF_ALU64 | BPF_MOV | BPF_K, BPF_REG_3, 0, 0, XDP_PASS},   // r3 = fallback
        {BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_redirect_map},  // r0 = redirect(...)
        {BPF_JMP | BPF_EXIT, 0, 0, 0, 0},
    };
    bpf_attr a{};
    a.prog_type = BPF_PROG_TYPE_XDP;
    a.insns = reinterpret_cast<std::uint64_t>(prog);
    a.insn_cnt = sizeof prog / sizeof prog[0];
    a.license = reinterpret_cast<std::uint64_t>("GPL");
    const int fd = static_cast<int>(bpf(BPF_PROG_LOAD, a));
    if (fd < 0) die("BPF_PROG_LOAD");
    return fd;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s ifname queue seconds\n", argv[0]);
        return 2;
    }
    const unsigned ifindex = if_nametoindex(argv[1]);
    const std::uint32_t queue = static_cast<std::uint32_t>(std::atoi(argv[2]));
    if (ifindex == 0) die("if_nametoindex");

    // 1. UMEM: one pinned region the NIC DMAs into, cut into 2 KiB frames (13.3, 7.4).
    void* umem = mmap(nullptr, std::size_t{kFrames} * kFrame, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (umem == MAP_FAILED) die("mmap umem");
    const int xsk = socket(AF_XDP, SOCK_RAW, 0);
    if (xsk < 0) die("socket(AF_XDP)");
    xdp_umem_reg reg{};
    reg.addr = reinterpret_cast<std::uint64_t>(umem);
    reg.len = std::uint64_t{kFrames} * kFrame;
    reg.chunk_size = kFrame;
    if (setsockopt(xsk, SOL_XDP, XDP_UMEM_REG, &reg, sizeof reg)) die("XDP_UMEM_REG");

    // 2. SPSC rings shared with the kernel: fill, completion, RX (a sender adds TX).
    const std::uint32_t n = kRing;
    for (int opt : {XDP_UMEM_FILL_RING, XDP_UMEM_COMPLETION_RING, XDP_RX_RING})
        if (setsockopt(xsk, SOL_XDP, opt, &n, sizeof n)) die("ring size");
    xdp_mmap_offsets off{};
    socklen_t olen = sizeof off;
    if (getsockopt(xsk, SOL_XDP, XDP_MMAP_OFFSETS, &off, &olen)) die("XDP_MMAP_OFFSETS");
    Ring fill = map_ring(xsk, off.fr, sizeof(std::uint64_t), XDP_UMEM_PGOFF_FILL_RING);
    map_ring(xsk, off.cr, sizeof(std::uint64_t), XDP_UMEM_PGOFF_COMPLETION_RING);
    Ring rx = map_ring(xsk, off.rx, sizeof(xdp_desc), XDP_PGOFF_RX_RING);

    // 3. Give the kernel every frame to receive into: we produce, the kernel consumes.
    auto* fill_addr = static_cast<std::uint64_t*>(fill.desc);
    for (std::uint32_t i = 0; i < kRing; ++i) fill_addr[i] = std::uint64_t{i} * kFrame;
    __atomic_store_n(fill.producer, kRing, __ATOMIC_RELEASE);

    // 4. Bind to one hardware queue; ask for zero-copy (fails if the driver can't).
    sockaddr_xdp sxdp{};
    sxdp.sxdp_family = AF_XDP;
    sxdp.sxdp_ifindex = ifindex;
    sxdp.sxdp_queue_id = queue;
    sxdp.sxdp_flags = XDP_ZEROCOPY | XDP_USE_NEED_WAKEUP;
    if (bind(xsk, reinterpret_cast<sockaddr*>(&sxdp), sizeof sxdp) != 0) {
        sxdp.sxdp_flags = XDP_COPY | XDP_USE_NEED_WAKEUP;     // kernel copies instead
        if (bind(xsk, reinterpret_cast<sockaddr*>(&sxdp), sizeof sxdp) != 0) die("bind");
        std::puts("zero-copy unavailable: running in copy mode");
    }

    // 5. Steer the queue's packets to the socket: XSKMAP[queue] = xsk, then attach.
    bpf_attr a{};
    a.map_type = BPF_MAP_TYPE_XSKMAP;
    a.key_size = a.value_size = 4;
    a.max_entries = 64;
    const int map = static_cast<int>(bpf(BPF_MAP_CREATE, a));
    if (map < 0) die("BPF_MAP_CREATE");
    a = {};
    a.map_fd = static_cast<std::uint32_t>(map);
    a.key = reinterpret_cast<std::uint64_t>(&queue);
    a.value = reinterpret_cast<std::uint64_t>(&xsk);
    if (bpf(BPF_MAP_UPDATE_ELEM, a)) die("BPF_MAP_UPDATE_ELEM");
    a = {};
    a.link_create.prog_fd = static_cast<std::uint32_t>(load_program(map));
    a.link_create.target_ifindex = ifindex;
    a.link_create.attach_type = BPF_XDP;
    a.link_create.flags = XDP_FLAGS_DRV_MODE;                 // native mode in the driver
    if (bpf(BPF_LINK_CREATE, a) < 0) {
        a.link_create.flags = XDP_FLAGS_SKB_MODE;              // generic: after the skb
        if (bpf(BPF_LINK_CREATE, a) < 0) die("BPF_LINK_CREATE"); // (no speed-up)
        std::puts("native XDP unavailable: generic mode");
    }                                                          // detached at exit

    // 6. The hot loop: busy-poll the RX ring, parse in place, recycle the frame.
    auto arb = std::make_unique<feed::Arbiter<>>(1, 32);
    std::uint64_t pkts = 0, msgs = 0;
    auto deliver = [&](std::uint64_t, std::span<const std::byte>) { ++msgs; };
    auto gap = [](std::uint64_t from, std::uint64_t to) {
        std::printf("gap [%llu, %llu)\n", static_cast<unsigned long long>(from),
                    static_cast<unsigned long long>(to));
    };
    const auto* desc = static_cast<const xdp_desc*>(rx.desc);
    const auto stop = std::chrono::steady_clock::now() +
                      std::chrono::seconds(std::atoi(argv[3]));
    std::uint32_t rx_cons = 0, fill_prod = kRing;
    while (std::chrono::steady_clock::now() < stop) {
        const std::uint32_t avail =
            __atomic_load_n(rx.producer, __ATOMIC_ACQUIRE) - rx_cons;   // wraps: fine
        if (avail == 0) {
            if (__atomic_load_n(fill.flags, __ATOMIC_RELAXED) & XDP_RING_NEED_WAKEUP)
                recvfrom(xsk, nullptr, 0, MSG_DONTWAIT, nullptr, nullptr);   // kick
            continue;
        }
        for (std::uint32_t i = 0; i < avail; ++i) {
            const xdp_desc d = desc[(rx_cons + i) & (kRing - 1)];
            const auto* f = static_cast<const std::byte*>(umem) + d.addr;
            // Ethernet (14) + IPv4 (IHL*4) + UDP (8); accept only IPv4/UDP.
            if (d.len > 14 + 20 + 8 && f[12] == std::byte{0x08} && f[13] == std::byte{0} &&
                f[14 + 9] == std::byte{IPPROTO_UDP}) {
                const std::size_t l4 = 14 + 4 * (std::to_integer<std::size_t>(f[14]) & 15);
                const std::size_t udp_len = feed::load_be16(f + l4 + 4);  // not d.len:
                if (udp_len >= 8 && l4 + udp_len <= d.len) {               // frames may
                    arb->on_packet({f + l4 + 8, udp_len - 8}, deliver, gap); // be padded
                    ++pkts;
                }
            }
            fill_addr[fill_prod++ & (kRing - 1)] = d.addr & ~std::uint64_t{kFrame - 1};
        }
        rx_cons += avail;
        __atomic_store_n(rx.consumer, rx_cons, __ATOMIC_RELEASE);      // slots free
        __atomic_store_n(fill.producer, fill_prod, __ATOMIC_RELEASE);  // frames back
    }
    std::printf("%llu UDP packets, %llu feed messages delivered\n",
                static_cast<unsigned long long>(pkts),
                static_cast<unsigned long long>(msgs));
}
