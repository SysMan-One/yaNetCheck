<!-- ident: X.01-28ECO03 -->

# YANETCHK -- a network path checking utility

Sends a paced stream of UDP datagrams (or raw Ethernet+IPv4+UDP frames
via AF_PACKET) with incrementing sequence numbers; the receiver detects
lost, duplicated and reordered packets.

Format:

```
$ yanetchk send     /TIP=<ip> [<qualifiers>]
$ yanetchk receive  [<qualifiers>]
$ yanetchk help [<topic> [<subtopic>]]
```

The operational mode is selected by the verb; /IFNAME switches the
engine from UDP to AF_PACKET (raw socket) in the both modes, which
requires CAP_NET_RAW or root.

Example -- a UDP receiver on one terminal and a UDP sender on another,
both on the loopback:

```
$ yanetchk receive /lpn=12347
%YANETCHK-I: Rev: X.01-28ECO03/x86_64, (built  at Nov 12 2026 22:13:19 with CC 12.2.0)
%YANETCHK-I: Mode is: Receiver
%YANETCHK-I: Payload size after validation: 128 octets
%YANETCHK-I: Delta: 10000000 nsec (0 sec, 10000000 nanosec)
%YANETCHK-I: Start <s_thread_receiver> on: 0.0.0.0:12347 ...

$ yanetchk send /tip=127.0.0.1 /tpn=12347 /delta=1000000
%YANETCHK-I: Rev: X.01-28ECO03/x86_64, (built  at Nov 12 2026 22:13:19 with CC 12.2.0)
%YANETCHK-I: Mode is: Sender
%YANETCHK-I: Payload size after validation: 128 octets
%YANETCHK-I: Delta: 1000000 nsec (0 sec, 1000000 nanosec)
%YANETCHK-I: Calculated load: 1000 pps, L2 1.360 Mbit/s (170 octets/frame), wire 1.552 Mbit/s (194 octets/frame incl. FCS+preamble+IFG)
%YANETCHK-I: Start <s_thread_sender> to: 127.0.0.1:12347 ...
```

Every line really starts with the date, the time and the thread id
(`12-11-2026 22:07:34.452 2569551 %YANETCHK-I: ...`); the examples here
leave them out.

Both sides then print a running tally every three seconds -- SENT by
the sender, STATS by the receiver (here the receiver line is its
second one: the first interval started before the sender did, so it
counted fewer packets):

```
%YANETCHK-I-SENT, Sent: +3000 pkts, total: 3000 pkts, Seq: 3001, rate: 999 pps, 1551 kbit/s (wire).
%YANETCHK-I-STATS, Rcvd: +3000 pkts, total: 4999 pkts, lost: +0/0, reord: +0/0 (max 0), dup: +0/0, Seq: 4999, delta: 999374 nsec, rate: 999 pps, 1551 kbit/s (wire).
```

# SEND

Transmits a paced stream of test packets to /TIP:/TPN. The pacing is
driven by /DELTA on the absolute CLOCK_MONOTONIC time base, so the
average rate does not drift. With /IFNAME the AF_PACKET engine forms
complete Ethernet+IPv4+UDP frames and injects them directly into the
interface; no ARP is performed, the destination MAC comes from /TMAC.

The sender survives receiver restarts: an ICMP Port Unreachable is a
transient condition and the transmission continues at the paced rate.
The sender prints nothing about it.

Examples:

```
$ yanetchk send /tip=192.168.1.10 /tpn=12347 /delta=1000000 /datalen=512
%YANETCHK-I: Mode is: Sender
%YANETCHK-I: Payload size after validation: 512 octets
%YANETCHK-I: Delta: 1000000 nsec (0 sec, 1000000 nanosec)
%YANETCHK-I: Calculated load: 1000 pps, L2 4.432 Mbit/s (554 octets/frame), wire 4.624 Mbit/s (578 octets/frame incl. FCS+preamble+IFG)
%YANETCHK-I: Start <s_thread_sender> to: 192.168.1.10:12347 ...
%YANETCHK-I-SENT, Sent: +3002 pkts, total: 5997 pkts, Seq: 5998, rate: 1000 pps, 4626 kbit/s (wire).
```

`Seq` of the sender is the next sequence number to send, so it is one
more than `total`.

An AF_PACKET sender (root) out of eth0, the frames addressed to the
router MAC; the source IP is taken from eth0 automatically:

```
# yanetchk send /ifname=eth0 /tip=192.168.1.10 /tpn=12347 /tmac=aa:04:00:de:c0:01
%YANETCHK-I: Mode is: Sender
%YANETCHK-I: IFname: <eth0>, IFindex: 2
%YANETCHK-I: LIP is taken from IF <eth0>: 192.168.1.1
%YANETCHK-I: Target MAC (next hop): aa:04:00:de:c0:01
%YANETCHK-I: Full frame size after validation: 128 octets
%YANETCHK-I: Delta: 10000000 nsec (0 sec, 10000000 nanosec)
%YANETCHK-I: Calculated load: 100 pps, L2 0.102 Mbit/s (128 octets/frame), wire 0.122 Mbit/s (152 octets/frame incl. FCS+preamble+IFG)
%YANETCHK-I: Start <s_thread_sender2>: IF: eth0 (index: 2), 192.168.1.1:12345 -> 192.168.1.10:12347, dst MAC: aa:04:00:de:c0:01, frame: 128 octets
```

The source port is /LPN, 12345 by default.

## TIP

`/TIP=<IPv4>` -- the target IP address. REQUIRED.

## TPN

`/TPN=<n>` -- the target UDP port. Default: 12345.

## LIP

`/LIP=<IPv4>` -- the source IP for the AF_PACKET engine. When omitted,
the primary IPv4 address of the /IFNAME interface is taken; an interface
with no IPv4 address is a configuration error then.

## LPN

`/LPN=<n>` -- the source UDP port for the AF_PACKET engine.
Default: 12345.

## DATALEN

`/DATALEN=<n>` -- the UDP payload length, or the FULL frame length in
the AF_PACKET mode. Default: 128; clamped to the sane range.

## DELTA

`/DELTA=<n>` -- the inter-packet pacing interval in nanoseconds.
Default: 10000000 (10 ms, 100 pkt/sec). At very small values the real
rate is limited by the syscall cost, roughly 60-70 kpps per core;
`/DELTA=0` on the AF_PACKET TX ring (`/RING`) removes the pacing entirely
and transmits at line rate. At start-up the sender prints the calculated
offered load for the chosen `/DELTA` and frame size -- the packet rate
and both the L2 and the wire (FCS + preamble + inter-frame gap)
bandwidth.

## IFNAME

`/IFNAME=<name>` -- the output interface name; enables the AF_PACKET
engine.

## TMAC

`/TMAC=<mac>` -- the destination (next hop) MAC address for the
AF_PACKET engine, in the aa:bb:cc:dd:ee:ff or aa-bb-cc-dd-ee-ff form,
typically the MAC of the router. To find it:

```
$ ping -c1 <gateway_ip> >/dev/null; ip neigh show <gateway_ip>
```

## TRACE

`/TRACE` -- extensible diagnostic output. Only a Debug build prints
the per-packet trace lines, for example the `send()->-1, errno: 111`
(ICMP Port Unreachable) line of the UDP sender for every packet while
no receiver listens. A Release build, as installed from a package, says
nothing about it.

## RING

`/RING` -- send through the TPACKET (PACKET_MMAP) TX ring instead of a
`sendto()` per frame. It works only with the AF_PACKET engine
(`/IFNAME`) and is ignored, with a warning, without it. Frames are
written into a memory-mapped ring and flushed to the kernel in batches
with a single system call for many frames, which raises the transmit
rate by roughly an order of magnitude. Speed follows `/DELTA`:
`/DELTA=0` transmits at line rate (fill every free slot, flush, repeat),
a non-zero `/DELTA` keeps the per-frame interval as usual. The TX ring
also sets PACKET_QDISC_BYPASS, so frames skip the kernel traffic-control
layer and go straight to the driver -- a best-effort optimisation that is
simply skipped (with a warning) on a kernel that does not support it.

```
# yanetchk send /ifname=eth0 /tip=192.168.1.10 /tpn=12347 /tmac=aa:04:00:de:c0:01 /delta=0 /ring
%YANETCHK-I: Calculated load: line rate (/DELTA=0, unbounded), L2 128 octets/frame, wire 152 octets/frame
%YANETCHK-I: Start <s_thread_sender3>: IF: eth0 (index: 2), 192.168.1.1:12345 -> 192.168.1.10:12347, frame: 128 octets, TX ring: 1024 frames, mode: line-rate
```

## LOGFILE

`/LOGFILE=<file>` -- a file to accept the logging; stdout by default.

## LOGSIZE

`/LOGSIZE=<n>` -- the maximum log file size in octets.

# RECEIVE

Listens for the test stream and accounts the received, lost, duplicated
and reordered packets by the sequence numbers. The very first valid
packet only synchronizes the sequence base, so a mid-stream start is not
a loss; a sequence rollback of more than 100000 is treated as a sender
restart and resynchronizes the base.

With /IFNAME the AF_PACKET engine is selected: a raw socket bound to the
interface with an in-kernel BPF filter. The frames are seen BEFORE the
local netfilter, which makes the two engines a firewall-versus-path loss
discriminator.

Examples:

```
$ yanetchk receive /lpn=12347
%YANETCHK-I: Mode is: Receiver
%YANETCHK-I: Payload size after validation: 128 octets
%YANETCHK-I: Delta: 10000000 nsec (0 sec, 10000000 nanosec)
%YANETCHK-I: Start <s_thread_receiver> on: 0.0.0.0:12347 ...
%YANETCHK-W-SEQSYNC, Sequence base synchronized, Seq: 1
%YANETCHK-I-STATS, Rcvd: +3000 pkts, total: 4999 pkts, lost: +0/0, reord: +0/0 (max 0), dup: +0/0, Seq: 4999, delta: 999374 nsec, rate: 999 pps, 1551 kbit/s (wire).
```

SEQSYNC is a warning on purpose: at the start it only says where the
receiver joined the stream, but in the middle of a run it marks a
break in the sequence. The "Payload size" and "Delta" lines show only
the built-in defaults: the receiver does not use them (and does not
accept /DATALEN or /DELTA).

An AF_PACKET receiver (root) on end0, the in-kernel BPF filtering by the
UDP port, plus a destination IP filter:

```
# yanetchk receive /ifname=end0 /lpn=12347 /lip=192.168.1.10
%YANETCHK-I: Mode is: Receiver
%YANETCHK-I: IFname: <end0>, IFindex: 7
%YANETCHK-I: Payload size after validation: 128 octets
%YANETCHK-I: Delta: 10000000 nsec (0 sec, 10000000 nanosec)
%YANETCHK-I: Start <s_thread_receiver2> on IF index: 7, UDP port: 12347 ...
```

With /VERBOSE, out-of-order and duplicate arrivals are reported as they
happen (a forward gap is only noted as pending, not yet lost):

```
%YANETCHK-I-SEQGAP, Seq gap: 49 -> 51 (1 ahead), pending
%YANETCHK-I-REORD, Rcvd Seq: 50 --- Reordered (1 behind), recovered
%YANETCHK-W-DUP, Rcvd Seq: 30 --- Duplicate detected
```

Each sequence event carries a VMS message keyword (SEQSYNC, SEQGAP,
REORD, DEEPREORD, DUP, RESTART) from the YANETCHK message catalogue.

## LIP

`/LIP=<IPv4>` -- the bind address of the UDP receiver (0.0.0.0 = all),
or an additional destination IP filter of the AF_PACKET receiver.

## LPN

`/LPN=<n>` -- the UDP port to listen or filter on. Default: 12345.

## IFNAME

`/IFNAME=<name>` -- the interface name; enables the AF_PACKET engine.

## RING

`/RING` -- receive through the TPACKET_V3 (PACKET_MMAP) RX ring instead of a
`recvfrom()` per frame. Works only with the AF_PACKET engine (`/IFNAME`)
and is ignored, with a warning, without it. Frames are read from a
memory-mapped ring, removing the per-packet system call that otherwise
makes the receiver drop most of a line-rate stream -- on loopback
against the TX-ring sender the loss fell roughly from 83% to 29%. The
BPF port filter, the `/LIP` check and the accounting are unchanged.

```
# yanetchk receive /ifname=end0 /lpn=12347 /ring
%YANETCHK-I: Start <s_thread_receiver3> on IF index: 7, UDP port: 12347, RX ring V3: 16 blocks x 256 KiB ...
```

## TRACE

`/TRACE` -- extensible diagnostic output.

## VERBOSE

`/VERBOSE` -- print the per-packet messages that are silent by default:
SEQGAP (a forward gap held pending), REORD (a recovered reorder) and DUP
(a duplicate detected). They are useful for watching a single flow, but flood the log
at high packet rates, so they are off unless `/VERBOSE` is given. The
counters are always maintained -- the reorder tally appears in the
periodic STATS and the final summary either way -- and the remaining
WARNING-level events (DEEPREORD, SEQSYNC, RESTART) always print.

## LOGFILE

`/LOGFILE=<file>` -- a file to accept the logging; stdout by default.

## LOGSIZE

`/LOGSIZE=<n>` -- the maximum log file size in octets.

# STATISTICS

A statistics line is printed every three seconds; in the receiver mode
it carries the loss, reorder and duplicate counters:

```
%YANETCHK-I-STATS, Rcvd: +197113 pkts, total: 89676009 pkts, lost: +0/112,
    reord: +3/57 (max 4), dup: +0/2, Seq: 90250913, delta: 2527 nsec,
    rate: 65704 pps, 79896 kbit/s (wire).
```

(It is one line on the screen; it is broken here to fit.)

Every counter is shown as `+interval/cumulative`. `lost` is the
confirmed loss (a gap that left the reordering window unseen); `reord`
is the reordered packets that arrived below the high water mark but
inside the window and were recovered -- these are NOT counted as loss;
`max` is the deepest reorder displacement seen; `dup` is the duplicates.
`Seq` is the high water mark; `delta` is the inter-arrival interval of
the LAST packet -- at high rates it looks bimodal because of the NIC
interrupt coalescing, the true average interval is the interval length
divided by the `+N` receive count. `rate` is the packet rate and the
wire bandwidth of the interval: the frame octets plus the FCS, the
preamble and the inter-frame gap (24 octets per frame). If the sender
started or stopped inside an interval, `rate` of that interval is lower
than the real rate.

A gap is held pending inside a sliding window of 4096 sequence numbers
and charged to `lost` only when it is evicted still unseen, so a late
out-of-order arrival that fills the gap is recovered as a reorder rather
than double counted. On shutdown the receiver prints a closing tally,
folding the still-pending sequences into the final loss:

```
%YANETCHK-I-FINAL, total: 200 pkts, lost: 1, reord: 2 (max 2), dup: 1, high Seq: 200.
```

When no packet arrives in a three-second interval the line is raised
to a WARNING (`%YANETCHK-W-NOTRAFFIC, ... no traffic received`) -- it flags a
silent path or a stopped sender, including a stream that stops mid-run.

A healthy steady state is `lost: +0/M` with a constant M; the
`Seq - total` difference of the receiver is constant while nothing is
being lost.

A healthy high-rate run over AF_PACKET, holding about 65 kpps with no
loss over millions of packets:

```
%YANETCHK-I-STATS, Rcvd: +197113 pkts, total: 89676009 pkts, lost: +0/112, reord: +3/57 (max 4), dup: +0/2, Seq: 90250913, delta: 2527 nsec, rate: 65704 pps, 79896 kbit/s (wire).
%YANETCHK-I-STATS, Rcvd: +196207 pkts, total: 89872216 pkts, lost: +0/112, reord: +0/57 (max 4), dup: +0/2, Seq: 90447120, delta: 60409 nsec, rate: 65402 pps, 79528 kbit/s (wire).
```

The cumulative 112 does not grow, and `delta` alternates between a few
microseconds and tens of them -- the NIC delivers packets in batches.

When the sender is restarted after more than 100000 packets, the
receiver sees the sequence go back by more than 100000, prints one
warning, resynchronizes and charges no loss for it:

```
%YANETCHK-W-RESTART, High Seq: 32375220 --> Rcvd Seq: 1 --- Sender restart detected, resynchronize
```

The counters (total, lost, reord, dup) then start again from zero for
the new session. A restart after fewer than 100000 packets is not seen
as a restart: each new packet is then reported as a deep reorder and
counted in `reord`, and near the old high Seq as a duplicate, until the
new sequence passes the old one:

```
%YANETCHK-W-DEEPREORD, Rcvd Seq: 1 is 39762 behind the window - deep reorder
%YANETCHK-W-DEEPREORD, Rcvd Seq: 2 is 39761 behind the window - deep reorder
```

Restart the receiver too in that case.

# SIGNALS

The first SIGTERM, SIGINT (Ctrl-C) or SIGQUIT requests a graceful
shutdown; the second one terminates the process immediately.

# HELP

The help is served from the installed YANETCHK.HLB library through the
external HELP browser. The library is built out of this Markdown source
by HELPTRAN, and YANETCHK runs the browser against it:

```
$ yanetchk help send tmac
$ yanetchk help receive
$ yanetchk send /HELP
```

The HELP browser image may be installed under a name other than `help`
(the HELP package uses `vhelp` to avoid the shell builtin). The build
bakes in the name it found; `$YANETCHK_HELP` overrides it at run time.

The library is located, in order, via the $YANETCHK_HLP environment
variable, then next to the binary, then on the system help path. When
neither the library nor the HELP browser can be found, a brief summary
generated from the command tables is shown instead, so /HELP always
answers.
