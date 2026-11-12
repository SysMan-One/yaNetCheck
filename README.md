# YANETCHK

[![CI](https://github.com/SysMan-One/yaNetCheck/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SysMan-One/yaNetCheck/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/SysMan-One/yaNetCheck)](https://github.com/SysMan-One/yaNetCheck/releases/latest)

Author: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)

A network path checking utility for Linux. Sends a paced stream of UDP datagrams (or raw Ethernet+IPv4+UDP frames via AF_PACKET) with incrementing sequence numbers; the receiver side detects and accounts lost, duplicated and reordered packets, and measures inter-arrival intervals.

Typical applications:

- verifying packet loss on a link, a switch, a router or a whole path at a controlled packet rate;
- comparing behaviour before/after a network reconfiguration;
- distinguishing "packets do not reach the wire" from "packets reach the interface but are dropped by the local firewall" (the AF_PACKET receiver sees frames before netfilter, the UDP receiver sees them after);
- long-run soak testing: the tool prints periodic statistics and survives sender/receiver restarts on either side.

Current revision: X.01-28ECO02 (`__IDENT__` of `src/yanetchk.c`; the build takes the version, 1.28.2, from it).

## Build

### Dependencies

- Linux with glibc (tested on x86_64 and aarch64), kernel 3.x or newer;
- GCC or Clang with C11 support (`_Atomic` is used);
- the StarLet package (UTILITY$ROUTINES, [SysMan-One/utility_routines](https://github.com/SysMan-One/utility_routines)). It is consumed as an installed CMake package rather than vendored in-tree: build and install it once, then point this build at its prefix. The CI builds against StarLet 1.7.2 (commit `7e910f8`);
- the external HELP package ([SysMan-One/vHELP](https://github.com/SysMan-One/vHELP): the librarian `vhelptran`/`helptran` and the browser `vhelp`/`help`): `doc/yanetchk.md` is compiled into `yanetchk.hlb` by the librarian, and `/HELP` delegates to the browser. yanetchk carries no help-reading code of its own anymore. The CI builds against HELP X.01-38ECO01 (commit `9f89cd6`).

Install the StarLet package first:

```
$ cmake -S <utility_routines> -B b -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
$ cmake --build b && cmake --install b
```

then build yanetchk against it (the headers come from the package, the code links `libstarlet.a`).


### Plain GCC

```
$ gcc -std=gnu11 -O2 -Wall -Wextra -D__ARCH__NAME__='"x86_64"' \
      -I<prefix>/include -o yanetchk yanetchk.c \
      -L<prefix>/lib -lstarlet -lpthread
```

where `<prefix>` is the StarLet install prefix.

### CMake (recommended, with install)

The project ships a `CMakeLists.txt` that finds the StarLet package, builds the utility, compiles `doc/yanetchk.md` into `yanetchk.hlb` with the HELP package's `helptran`, and installs both. The install directory of the library is baked into the binary, so `/HELP` finds it from anywhere.

```
$ cmake -S . -B build -DCMAKE_PREFIX_PATH=<starlet-prefix> \
        -DCMAKE_INSTALL_PREFIX=/usr/local
$ cmake --build build
$ ctest --test-dir build
$ cmake --install build
```

The librarian (`vhelptran` or `helptran`) and the browser (`vhelp` or `help`) of the HELP package must be on the `PATH` or under `CMAKE_PREFIX_PATH` at configure time. Without the librarian the library is not built and `/HELP` falls back to the built-in table summary; the browser is a run-time dependency looked up on the `PATH`.

This installs `bin/yanetchk` and `share/yanetchk/yanetchk.hlb`.

### Cross compilation for ARM targets

```
$ aarch64-linux-gnu-gcc -std=gnu11 -O2 -D__ARCH__NAME__='"aarch64"' \
      -I<prefix>/include -o yanetchk yanetchk.c \
      -L<prefix>/lib -lstarlet -lpthread
```

where `<prefix>` is the StarLet install prefix.

## Operational modes

The mode is selected by the command verb; /IFNAME switches the engine from UDP to AF_PACKET within the both modes:

| Verb    | /IFNAME | Resulting engine                                   | Privileges        |
|---------|---------|----------------------------------------------------|-------------------|
| receive | absent  | UDP receiver (listens on LIP:LPN)                  | none              |
| receive | given   | AF_PACKET receiver (raw socket on the interface)   | CAP_NET_RAW/root  |
| send    | absent  | UDP sender (transmits to TIP:TPN)                  | none              |
| send    | given   | AF_PACKET sender (hand-made frames to the wire)    | CAP_NET_RAW/root  |

The UDP engines use the normal kernel IP stack (routing, ARP, netfilter). The AF_PACKET engines bypass most of it: the sender injects complete, manually formed Ethernet+IPv4+UDP frames directly into the interface, the receiver taps frames from the interface before the netfilter INPUT chains.

## Command line and the built-in help

The command line follows the DCL convention: a verb followed by /QUALIFIER=value items (the `-qualifier=value` form is accepted as well). Qualifier names are case-insensitive and may be abbreviated to an unambiguous prefix; an exact name always wins over an abbreviation of a longer one.

The help is asked for like this:

```
$ yanetchk                  # no arguments: the help overview
$ yanetchk /HELP            # the same (also -h, --help)
$ yanetchk send /HELP       # the SEND command
$ yanetchk receive /HELP    # the RECEIVE command
```

A help request is detected before the parameter validation, so `yanetchk send /HELP` works even though /TIP is required for a real run.

The help text lives in `yanetchk.md` (the HELP package's Markdown format) and is compiled into a `yanetchk.hlb` library by `helptran` at build time. At runtime `/HELP` fork/execs the external HELP browser (`vhelp`, the name found at build time; `$YANETCHK_HELP` overrides it) against that library, passing the topic path words, so topic paths work and may be abbreviated:

```
$ yanetchk help send tmac       # the /TMAC topic of the SEND command
$ yanetchk help receive         # the RECEIVE command
$ yanetchk send /HELP           # the SEND command, as a switch
```

The library is located, in order, via `$YANETCHK_HLP`, then next to the binary, then the build-time `YNETCHK$K_HLBDIR`, then `./yanetchk.hlb`. The browser is looked up next to the binary first, then on the `PATH`. When neither the library nor the browser can be found, a brief summary generated from the command tables is shown instead, so `/HELP` always answers.

Building the help library by hand (the CMake build does it; needs the HELP package's librarian on the `PATH`):

```
$ vhelptran doc/yanetchk.md /OUTPUT=yanetchk.hlb /IDENT=X.01-28ECO02
```

Install `yanetchk.hlb` and the HELP browser next to the `yanetchk` binary, or point `$YANETCHK_HLP` at the library.

### Qualifiers of `send`

| Qualifier         | Default  | Meaning |
|-------------------|----------|---------|
| /TIP=\<IPv4\>     | REQUIRED | target IP address |
| /TPN=\<n\>        | 12345    | target UDP port |
| /LIP=\<IPv4\>     | auto     | source IP for the AF_PACKET engine; when omitted, taken from the primary IPv4 of /IFNAME |
| /LPN=\<n\>        | 12345    | source UDP port for the AF_PACKET engine |
| /DATALEN=\<n\>    | 128      | UDP: payload length in octets; AF_PACKET: FULL frame length (clamped to the sane range) |
| /DELTA=\<n\>      | 10000000 | inter-packet pacing interval, nanoseconds (10000000 ns = 10 ms = 100 pkt/sec); `/DELTA=0` with `/RING` is line rate |
| /IFNAME=\<name\>  | -        | interface name; enables the AF_PACKET engine |
| /TMAC=\<mac\>     | built-in | destination (next hop) MAC, `aa:bb:cc:dd:ee:ff` or `aa-bb-cc-dd-ee-ff`; typically the router MAC |
| /RING             | off      | send through the TPACKET (PACKET_MMAP) TX ring, many frames per system call, with PACKET_QDISC_BYPASS where the kernel has it; needs /IFNAME, ignored with a warning without it; speed follows /DELTA |
| /TRACE            | off      | extensible diagnostic output (in a Debug build also the per-packet ECONNREFUSED lines of the UDP sender) |
| /LOGFILE=\<file\> | stdout   | file to accept logging |
| /LOGSIZE=\<n\>    | -        | maximum log file size in octets |

### Qualifiers of `receive`

| Qualifier         | Default  | Meaning |
|-------------------|----------|---------|
| /LIP=\<IPv4\>     | 0.0.0.0  | bind address (UDP) or an additional destination IP filter (AF_PACKET) |
| /LPN=\<n\>        | 12345    | UDP port to listen/filter on |
| /IFNAME=\<name\>  | -        | interface name; enables the AF_PACKET engine |
| /RING             | off      | receive through the TPACKET_V3 (PACKET_MMAP) RX ring instead of one `recvfrom()` per frame; needs /IFNAME, ignored with a warning without it |
| /TRACE            | off      | extensible diagnostic output |
| /VERBOSE          | off      | print the per-packet SEQGAP, REORD and DUP messages, silent by default; the counters are kept either way |
| /LOGFILE=\<file\> | stdout   | file to accept logging |
| /LOGSIZE=\<n\>    | -        | maximum log file size in octets |

NB: the /CONFIG file facility of the pre-X.01 revisions is retired together with the old option engine; keep the launch line in a shell script instead.

## Examples

UDP receiver on all addresses, port 12347:

```
$ yanetchk receive /lpn=12347
```

UDP sender, 1000 pkt/sec, 512-octet payload:

```
$ yanetchk send /tip=192.168.1.10 /tpn=12347 /delta=1000000 /datalen=512
```

AF_PACKET sender (root), full 128-octet frames out of eth0, addressed to the router MAC, the source IP is taken from eth0 automatically:

```
# yanetchk send /ifname=eth0 /tip=192.168.1.10 /tpn=12347 /tmac=aa:04:00:de:c0:01
```

AF_PACKET receiver (root) on end0, in-kernel BPF filter by port 12347, additional destination IP filter:

```
# yanetchk receive /ifname=end0 /lpn=12347 /lip=192.168.1.10
```

How to find the next hop MAC for `/TMAC`:

```
$ ping -c1 <gateway_ip> >/dev/null; ip neigh show <gateway_ip>
```

To stop: the first SIGTERM/SIGINT/SIGQUIT (Ctrl-C) requests a graceful shutdown, the second one terminates the process immediately.

## Interpreting the results

### The periodic statistics line

Printed every ~3 seconds by the both sides - STATS by the receiver, SENT by the sender:

```
%YANETCHK-I-STATS, Rcvd: +2499 pkts, total: 2499 pkts, lost: +0/0, reord: +0/0 (max 0), dup: +0/0, Seq: 2499, delta: 1031825 nsec, rate: 832 pps, 1292 kbit/s (wire).
%YANETCHK-I-SENT, Sent: +2998 pkts, total: 2998 pkts, Seq: 2999, rate: 999 pps, 1550 kbit/s (wire).
```

| Field       | Meaning |
|-------------|---------|
| +N pkts     | packets received/sent during the last ~3-second interval |
| total       | cumulative packets since the start |
| lost: +N/M  | N = packets lost during the last interval, M = cumulative loss; a gap is charged only when it leaves the 4096-packet reorder window unseen; a healthy steady state is `+0/M` with a constant M |
| reord: +N/M (max D) | packets that arrived late but inside the window - recovered, NOT counted as loss; D is the deepest displacement seen |
| dup: +N/M   | duplicates (the same Seq seen twice) |
| Seq         | the high water mark (receiver) or the last sent (sender) sequence number |
| delta       | the inter-arrival interval of the LAST packet of the interval, nanoseconds (receiver only) |
| rate        | the measured packet rate and wire bandwidth of the interval (frame octets plus FCS, preamble and inter-frame gap) |

An interval with no packet at all is raised to a WARNING, `%YANETCHK-W-NOTRAFFIC`; on shutdown the receiver prints the closing tally `%YANETCHK-I-FINAL`, where the still-pending gaps are counted as lost.

Useful derived checks:

- `Seq - total` on the receiver is constant while nothing is being lost; it equals the packets sent before the receiver started plus the cumulative loss. If this difference grows between lines, packets are being lost right now (and the `lost` counter must grow accordingly).
- The loss ratio is `M / (total + M)`; e.g. 112 lost against 94 million received is about 1.2e-6.

### The delta field and its bimodality

`delta` is a single-packet sample, not an average, so at high rates it looks bimodal: a few microseconds most of the time with occasional jumps to tens of microseconds. This is normal and reflects NAPI/interrupt coalescing - the NIC delivers packets to the kernel in batches, so intra-batch gaps are tiny and inter-batch gaps are larger. The true average interval is `interval_seconds / +N` (e.g. 3 s / 197113 = ~15 us), not the printed delta.

### Event messages

Each event is a message of the YANETCHK catalogue, `%YANETCHK-<severity>-<IDENT>, text`. SEQGAP, REORD and DUP are printed only with `/VERBOSE`; the others always.

| Message | Meaning | Action |
|---------|---------|--------|
| `-W-SEQSYNC, Sequence base synchronized, Seq: N` | the first valid packet after the receiver start (the receiver joined the stream at seq N), or a resynchronization | none at the start; in the middle of a run it marks a discontinuity |
| `-I-SEQGAP, Seq gap: A -> B (K ahead), pending` | a forward gap; the missing packets are held pending, not yet lost | none by itself: a lost packet shows up in `lost` when it leaves the window |
| `-I-REORD, Rcvd Seq: N --- Reordered (K behind), recovered` | a late packet filled a pending gap; counted in `reord`, not in `lost` | genuine network reordering: multipath, bonding, QoS queues |
| `-W-DEEPREORD, Rcvd Seq: N is K behind the window - deep reorder` | a packet older than the 4096-packet window | heavy reordering or a delayed path; such a packet was already charged as lost |
| `-W-DUP, Rcvd Seq: N --- Duplicate detected` | the same seq arrived twice | usually a L2 loop or a misbehaving bonding/mirroring setup |
| `-W-RESTART, High Seq: A --> Rcvd Seq: B --- Sender restart detected, resynchronize` | a deep seq rollback (more than 100000); the sender was restarted, the base is resynchronized, no loss is charged | none, informational |
| `-W-NOTRAFFIC, Rcvd: +0 pkts in the last interval - no traffic received, ...` | nothing arrived in the last ~3 seconds | check that the sender runs and that the path (routes, firewall, /LIP, /LPN) is right |

### Where the loss happened: UDP vs AF_PACKET receiver

Run the receiver in the both modes against the same sender stream:

- the AF_PACKET receiver counts a packet as soon as the frame reaches the interface, BEFORE the local firewall;
- the UDP receiver counts it only after the full IP stack, netfilter included.

If the AF_PACKET receiver shows no loss while the UDP receiver does, the packets are dropped locally (netfilter rules, socket buffer overflow, rp_filter); check `nft list ruleset` / `iptables -L -v`, `ss -u -m`, and consider increasing `net.core.rmem_max`. If the both show the same loss, the packets never reached the host: look at the sender side, the switches and the counters `ip -s link show <if>` (RX errors/dropped/overrun) along the path.

### Startup and restart semantics

- The receiver may be started at any moment: the first valid packet only synchronizes the sequence base, the pre-start traffic is NOT counted as loss.
- The sender may be restarted at any moment: the receiver detects the deep sequence rollback, prints a single warning and resynchronizes; the loss counters of the previous session are kept, the new session starts clean.
- The sender survives receiver restarts: ICMP Port Unreachable is treated as a transient condition, the paced transmission continues, and the receiver will account the real gap on return. The sender says nothing about it in a Release build (in a Debug build /TRACE shows a `send()->-1, errno: 111 (ICMP Port Unreachable...)` line per packet).

## Implementation notes

- The wire format is a 16-octet payload: an 8-octet magic marker ("StarLet") plus a 64-bit sequence number starting from 1; everything without the magic or shorter than the payload is silently ignored by the receivers, so foreign traffic on the port does not distort the statistics.
- The pacing uses `clock_nanosleep(TIMER_ABSTIME)` on CLOCK_MONOTONIC, so the average rate does not drift and is immune to NTP steps. At very small `/DELTA` values the real rate is limited by the syscall cost (roughly 60-70 kpps per core for the plain send path).
- The AF_PACKET receiver installs an in-kernel classic BPF filter (IPv4, UDP, non-fragmented, destination port), so unrelated traffic never reaches the userspace; locally originated frames are skipped by `sll_pkttype`, and the payload offset honors the actual IP header length (IHL), so IP options do not break the parsing.
- The AF_PACKET sender forms the complete frame itself: the source MAC is taken from the interface (SIOCGIFHWADDR), the IP header checksum is computed, the UDP checksum is zero (permitted for IPv4). No ARP is performed - the destination MAC comes from `/TMAC`.
- The statistics counters are C11 `_Atomic`, safe on the both 64- and 32-bit targets.
- The command line engine is the StarLet CLI_ROUTINES library: the verb tables drive the parsing, the type validation (/TMAC is checked by the library `CLI$K_MAC` type, /TIP and /LIP by `CLI$K_IPV4`) and the fallback help summary.
- SCHED_FIFO is intentionally NOT used: with a small `/DELTA` a paced sender degenerates into a real-time busy loop and can starve a single/weak core system completely.

## Limitations

- IPv4 only.
- One stream (one sender, one receiver) per process instance.
- The AF_PACKET sender does not fragment: keep `/DATALEN` within the interface MTU + 14.
- The one-way latency is not measured (the payload carries no timestamps); `delta` characterizes the arrival process, not the path delay.

## Releases

A tag that is `__IDENT__` of `src/yanetchk.c`, exactly (`X.01-28ECO02`: a dot after the letter, two digits in every field, no `v`), makes a GitHub Release by `.github/workflows/release.yml`: the product is built on Debian 12 for amd64 and arm64, its tests are run, the DEB `yanetchk_<version>_<arch>.deb` (prefix `/usr`; the version follows from the ident, X.01-28ECO02 -> 1.28.2) is installed on Debian 12, Debian 13 and Ubuntu 24.04, run and removed, and the packages, the sources tarball `yanetchk-<ident>.tar.gz` and `SHA256SUMS` with its signature are attached. Any other tag is refused before the build.

```
$ sudo apt install ./yanetchk_1.28.2_amd64.deb
```

The package installs `/usr/bin/yanetchk` and `/usr/share/yanetchk/yanetchk.hlb`; the HELP package is suggested, not required - without its browser `/HELP` shows the summary of the command tables.

Every push to `main` and every pull request is built and tested (x86_64 and aarch64, `ctest`) by `.github/workflows/ci.yml`, the badge at the top.

## Checking a download

A release carries `SHA256SUMS.minisig`, a [minisign](https://jedisct1.github.io/minisign/) signature of `SHA256SUMS`: one signature for every package and the sources tarball. The public key is in `minisign.pub` here (the same key signs the kits of StarLet, HELP and AlphaEmu):

    RWQl4NAqFsHKkNoHkS52WSZSk+2kb+e58DWYpoi/jVFa6TqOTAm3Ph14

Put the files you took next to `SHA256SUMS` and `SHA256SUMS.minisig`, then:

    minisign -Vm SHA256SUMS -P RWQl4NAqFsHKkNoHkS52WSZSk+2kb+e58DWYpoi/jVFa6TqOTAm3Ph14
    sha256sum -c --ignore-missing SHA256SUMS

The first must say `Signature and comment signature verified`, the second `OK` for every file. Anything else - the file is not the one released: do not install it.

## License

Apache License 2.0 - see [LICENSE](LICENSE) and [NOTICE](NOTICE).
