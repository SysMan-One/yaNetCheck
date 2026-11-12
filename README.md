# YANETCHK

[![CI](https://github.com/SysMan-One/yaNetCheck/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SysMan-One/yaNetCheck/actions/workflows/ci.yml)

Author: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)

A network path checking utility for Linux. Sends a paced stream of UDP datagrams (or raw Ethernet+IPv4+UDP frames via AF_PACKET) with incrementing sequence numbers; the receiver side detects and accounts lost, duplicated and reordered packets, and measures inter-arrival intervals.

Typical applications:

- verifying packet loss on a link, a switch, a router or a whole path at a controlled packet rate;
- comparing behaviour before/after a network reconfiguration;
- distinguishing "packets do not reach the wire" from "packets reach the interface but are dropped by the local firewall" (the AF_PACKET receiver sees frames before netfilter, the UDP receiver sees them after);
- long-run soak testing: the tool prints periodic statistics and survives sender/receiver restarts on either side.

Current revision: X.01-02.

## Build

### Dependencies

- Linux with glibc (tested on x86_64 and aarch64), kernel 3.x or newer;
- GCC or Clang with C11 support (`_Atomic` is used);
- the StarLet package (UTILITY$ROUTINES, revision of 01-AUG-2026 or later - utility_routines V.01-24 (StarLet 1.2.2), cli_routines X.00-16, starlet X.01-12). It is consumed as an installed CMake package rather than vendored in-tree: build and install it once, then point this build at its prefix. V.01-13/14 fix two stack overflows in the `$LOG` path; V.01-17 is the current baseline;
- the external HELP package (the HELPTRAN librarian and the HELP browser): `yanetchk.md` is compiled into `yanetchk.hlb` by `helptran`, and `/HELP` delegates to the `help` browser. yanetchk carries no help-reading code of its own anymore.

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

`helptran` and the `help` browser (the HELP package) must be on the `PATH` or under `CMAKE_PREFIX_PATH` at configure time. Without `helptran` the library is not built and `/HELP` falls back to the built-in table summary; the `help` browser is a run-time dependency looked up on the `PATH`.

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

The help is built in and generated from the command tables, so it never gets out of sync with the code:

```
$ yanetchk                  # no arguments: the list of available commands
$ yanetchk /HELP            # the same (also -h, --help)
$ yanetchk send /HELP       # the SYNOPSIS of 'send': qualifiers, types,
$ yanetchk receive /HELP    #   defaults and the required flags
```

A help request is detected before the parameter validation, so `yanetchk send /HELP` works even though /TIP is required for a real run.

The help text lives in `yanetchk.md` (the HELP package's Markdown format) and is compiled into a `yanetchk.hlb` library by `helptran` at build time. At runtime `/HELP` fork/execs the external `help` browser against that library, passing the topic path words, so topic paths work and may be abbreviated:

```
$ yanetchk help send tmac       # the /TMAC topic of the SEND command
$ yanetchk help receive         # the RECEIVE command
$ yanetchk send /HELP           # the SEND command, as a switch
```

The library is located, in order, via `$YANETCHK_HLP`, then next to the binary, then the build-time `YNETCHK$K_HLBDIR`, then `./yanetchk.hlb`. The browser is looked up next to the binary first, then on the `PATH`. When neither the library nor the browser can be found, a brief summary generated from the command tables is shown instead, so `/HELP` always answers.

Building the help library (needs the HELP package's `helptran` on the `PATH`):

```
$ helptran yanetchk.md /OUTPUT=yanetchk.hlb /IDENT=X-01
```

Install `yanetchk.hlb` and the `help` browser next to the `yanetchk` binary, or point `$YANETCHK_HLP` at the library.

### Qualifiers of `send`

| Qualifier         | Default  | Meaning |
|-------------------|----------|---------|
| /TIP=\<IPv4\>     | REQUIRED | target IP address |
| /TPN=\<n\>        | 12345    | target UDP port |
| /LIP=\<IPv4\>     | auto     | source IP for the AF_PACKET engine; when omitted, taken from the primary IPv4 of /IFNAME |
| /LPN=\<n\>        | 12345    | source UDP port for the AF_PACKET engine |
| /DATALEN=\<n\>    | 128      | UDP: payload length in octets; AF_PACKET: FULL frame length (clamped to the sane range) |
| /DELTA=\<n\>      | 10000    | inter-packet pacing interval, microseconds (10000 us = 100 pkt/sec) |
| /IFNAME=\<name\>  | -        | interface name; enables the AF_PACKET engine |
| /TMAC=\<mac\>     | built-in | destination (next hop) MAC, `aa:bb:cc:dd:ee:ff` or `aa-bb-cc-dd-ee-ff`; typically the router MAC |
| /TRACE            | off      | extensible diagnostic output (also unmutes the per-packet ECONNREFUSED warnings) |
| /LOGFILE=\<file\> | stdout   | file to accept logging |
| /LOGSIZE=\<n\>    | -        | maximum log file size in octets |

### Qualifiers of `receive`

| Qualifier         | Default  | Meaning |
|-------------------|----------|---------|
| /LIP=\<IPv4\>     | 0.0.0.0  | bind address (UDP) or an additional destination IP filter (AF_PACKET) |
| /LPN=\<n\>        | 12345    | UDP port to listen/filter on |
| /IFNAME=\<name\>  | -        | interface name; enables the AF_PACKET engine |
| /TRACE            | off      | extensible diagnostic output |
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
$ yanetchk send /tip=192.168.1.10 /tpn=12347 /delta=1000 /datalen=512
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

Printed every ~3 seconds by the both sides:

```
Rcvd: +197113 pkts, total: 89676009 pkts, lost: +0/112 pkts, Seq: 90250913, delta: 2527 nsec.
```

| Field       | Meaning |
|-------------|---------|
| Rcvd / Sent | the operational mode of this instance |
| +N pkts     | packets received/sent during the last ~3-second interval; divide by 3 for the pps rate (+197113 = ~65.7 kpps) |
| total       | cumulative packets since the start |
| lost: +N/M  | N = packets lost during the last interval, M = cumulative loss; a healthy steady state is `+0/M` with a constant M |
| Seq         | the last seen (receiver) or sent (sender) sequence number |
| delta       | the inter-arrival interval of the LAST packet of the interval, nanoseconds (receiver mode; meaningless on the sender) |

Useful derived checks:

- `Seq - total` on the receiver is constant while nothing is being lost; it equals the packets sent before the receiver started plus the cumulative loss. If this difference grows between lines, packets are being lost right now (and the `lost` counter must grow accordingly).
- The loss ratio is `M / (total + M)`; e.g. 112 lost against 94 million received is about 1.2e-6.

### The delta field and its bimodality

`delta` is a single-packet sample, not an average, so at high rates it looks bimodal: a few microseconds most of the time with occasional jumps to tens of microseconds. This is normal and reflects NAPI/interrupt coalescing - the NIC delivers packets to the kernel in batches, so intra-batch gaps are tiny and inter-batch gaps are larger. The true average interval is `interval_seconds / +N` (e.g. 3 s / 197113 = ~15 us), not the printed delta.

### Event messages

| Message | Meaning | Action |
|---------|---------|--------|
| `Sequence base synchronized, Seq: N` | the first valid packet after the receiver start; the receiver joined an already running stream at seq N | none, informational |
| `Lost K packets` | a gap in the sequence numbers; K is added to the cumulative loss | investigate if it repeats: link errors, queue drops, CPU starvation on either side |
| `Duplicate detected` | the same seq arrived twice | usually a L2 loop or a misbehaving bonding/mirroring setup |
| `Old packet detected` | a shallow seq rollback (up to 100000) | genuine network reordering: multipath, bonding, QoS queues |
| `Sender restart detected, resynchronize` | a deep seq rollback (more than 100000); the sender was restarted, the base is resynchronized, no loss is charged | none, informational |
| `send()->-1, errno: 111 (ICMP Port Unreachable...)` | the UDP receiver is not listening; visible under /TRACE only | start the receiver; the sender keeps sending anyway |

### Where the loss happened: UDP vs AF_PACKET receiver

Run the receiver in the both modes against the same sender stream:

- the AF_PACKET receiver counts a packet as soon as the frame reaches the interface, BEFORE the local firewall;
- the UDP receiver counts it only after the full IP stack, netfilter included.

If the AF_PACKET receiver shows no loss while the UDP receiver does, the packets are dropped locally (netfilter rules, socket buffer overflow, rp_filter); check `nft list ruleset` / `iptables -L -v`, `ss -u -m`, and consider increasing `net.core.rmem_max`. If the both show the same loss, the packets never reached the host: look at the sender side, the switches and the counters `ip -s link show <if>` (RX errors/dropped/overrun) along the path.

### Startup and restart semantics

- The receiver may be started at any moment: the first valid packet only synchronizes the sequence base, the pre-start traffic is NOT counted as loss.
- The sender may be restarted at any moment: the receiver detects the deep sequence rollback, prints a single warning and resynchronizes; the loss counters of the previous session are kept, the new session starts clean.
- The sender survives receiver restarts: ICMP Port Unreachable is treated as a transient condition, the paced transmission continues, and the receiver will account the real gap on return.

## Implementation notes

- The wire format is a 16-octet payload: an 8-octet magic marker ("StarLet") plus a 64-bit sequence number starting from 1; everything without the magic or shorter than the payload is silently ignored by the receivers, so foreign traffic on the port does not distort the statistics.
- The pacing uses `clock_nanosleep(TIMER_ABSTIME)` on CLOCK_MONOTONIC, so the average rate does not drift and is immune to NTP steps. At very small `/DELTA` values the real rate is limited by the syscall cost (roughly 60-70 kpps per core for the plain send path).
- The AF_PACKET receiver installs an in-kernel classic BPF filter (IPv4, UDP, non-fragmented, destination port), so unrelated traffic never reaches the userspace; locally originated frames are skipped by `sll_pkttype`, and the payload offset honors the actual IP header length (IHL), so IP options do not break the parsing.
- The AF_PACKET sender forms the complete frame itself: the source MAC is taken from the interface (SIOCGIFHWADDR), the IP header checksum is computed, the UDP checksum is zero (permitted for IPv4). No ARP is performed - the destination MAC comes from `/TMAC`.
- The statistics counters are C11 `_Atomic`, safe on the both 64- and 32-bit targets.
- The command line engine is the StarLet CLI_ROUTINES library: the verb tables drive the parsing, the type validation (/TMAC is checked by the library `CLI$K_MAC` type, /TIP and /LIP by `CLI$K_IPV4`) and the built-in help generation.
- SCHED_FIFO is intentionally NOT used: with a small `/DELTA` a paced sender degenerates into a real-time busy loop and can starve a single/weak core system completely.

## Limitations

- IPv4 only.
- One stream (one sender, one receiver) per process instance.
- The AF_PACKET sender does not fragment: keep `/DATALEN` within the interface MTU + 14.
- `/TMAC` accepts the colon-separated form only.
- The one-way latency is not measured (the payload carries no timestamps); `delta` characterizes the arrival process, not the path delay.

## License

Apache License 2.0 - see [LICENSE](LICENSE) and [NOTICE](NOTICE).
