#define	__MODULE__	"YANETCHK"
#define	__IDENT__	"X.01-28ECO03"
#define	__REV__		"1.28.3"

/*
**++
**
**  FACILITY:	YANETCHK - a network path checking utility (UDP and AF_PACKET
**		sender/receiver with sequence tracking and loss accounting)
**
**  ABSTRACT:	Sends a stream of UDP datagrams (or raw Ethernet+IPv4+UDP frames
**		via AF_PACKET) with incrementing sequence numbers;
**		the receiver side detects lost, duplicated and reordered packets.
**
**  USAGE:
**
**	The operational mode is selected by the command verb:
**	  $ yanetchk send     /TIP=<ip> [<qualifiers>]	- the sender
**	  $ yanetchk receive  [<qualifiers>]		- the receiver
**	  $ yanetchk /HELP  (also -h, --help, or a verb followed by /HELP)
**
**	/IFNAME switches the engine from UDP to AF_PACKET (raw socket) in the
**	both modes; the AF_PACKET engines require CAP_NET_RAW or root.
**
**	Qualifiers of 'send':
**	  /TIP=<IPv4>		target IP address (REQUIRED)
**	  /TPN=<n>		target UDP port (default: 12345)
**	  /LIP=<IPv4>		source IP for the AF_PACKET engine; when omitted,
**				taken from the primary IPv4 of /IFNAME
**	  /LPN=<n>		source UDP port for the AF_PACKET engine (default: 12345)
**	  /DATALEN=<n>		UDP: payload length; AF_PACKET: FULL frame length
**				(default: 128, clamped to the sane range)
**	  /DELTA=<n>		inter-packet pacing interval, nanoseconds
**				(default: 10000000 = 10 ms, i.e. 100 pkt/sec)
**	  /IFNAME=<name>	interface name, enables the AF_PACKET engine
**	  /TMAC=<mac>		destination (next hop) MAC, aa:bb:cc:dd:ee:ff or
**				aa-bb-cc-dd-ee-ff (default: the built-in g_tmac)
**	  /RING			transmit through the AF_PACKET TPACKET TX ring
**				(needs /IFNAME; /DELTA=0 => line rate, else paced)
**	  /TRACE, /LOGFILE=<file>, /LOGSIZE=<n>
**
**	Qualifiers of 'receive':
**	  /LIP=<IPv4>		bind address (UDP) or destination IP filter (AF_PACKET)
**	  /LPN=<n>		UDP port to listen/filter on (default: 12345)
**	  /IFNAME=<name>	interface name, enables the AF_PACKET engine
**	  /RING			receive through the AF_PACKET TPACKET RX ring (needs /IFNAME)
**	  /VERBOSE		print the per-packet SEQGAP/REORD/DUP info messages (off by default)
**	  /TRACE, /LOGFILE=<file>, /LOGSIZE=<n>
**
**	Qualifier names may be abbreviated to an unambiguous prefix, an exact
**	name always wins over an abbreviation; the case is ignored.
**	NB: the /CONFIG file facility of the pre-X.01 revisions is retired.
**
**	Examples:
**
**	  UDP receiver on all addresses, port 12347:
**	    $ yanetchk receive /lpn=12347
**
**	  UDP sender, 1000 pkt/sec, 512-octet payload:
**	    $ yanetchk send /tip=192.168.1.10 /tpn=12347 /delta=1000000 /datalen=512
**
**	  AF_PACKET sender (root), the source IP is taken from eth0:
**	    $ yanetchk send /ifname=eth0 /tip=192.168.1.10 /tpn=12347 /tmac=aa:04:00:de:c0:01
**
**	  AF_PACKET receiver (root), in-kernel BPF by port, destination IP filter:
**	    $ yanetchk receive /ifname=end0 /lpn=12347 /lip=192.168.1.10
**
**	Runtime statistics is printed every ~3 seconds:
**	    Rcvd: +85195 pkts, total: 686063 pkts, lost: +0/217993 pkts, Seq: 904056, delta: 54640 nsec, rate: 28398 pps, 350 Mbit/s (wire).
**	where '+N' are the per-interval increments, 'lost: +N/M' is the
**	per-interval/cumulative loss, 'delta' is the last inter-arrival
**	interval (receiver mode), and 'rate' is the measured throughput over
**	the interval - the packet rate and the wire bandwidth (frame octets
**	plus FCS/preamble/IFG). The receiver measures the octets actually
**	received; the transmitter uses its configured frame size.
**
**	The first SIGTERM/SIGINT/SIGQUIT requests a graceful shutdown,
**	the second one terminates the process immediately.
**
**  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:	 1-JUL-2026
**
**  MODIFICATION HISTORY:
**
**	 1-JUL-2026	RRL	X.00-01		Initial version.
**
**	24-JUL-2026	RRL	X.00-02		Audit fixes:
**					- AF_PACKET sender forms a full Ethernet+IPv4+UDP frame
**					  (src MAC via SIOCGIFHWADDR, IP header checksum);
**					- frame/payload length validation is mode dependent;
**					- receiver validates length and magic of incoming datagram;
**					- statistic counters are C11 _Atomic (no data race);
**					- single volatile sig_atomic_t g_exit_flag;
**					- async-signal-safe signal handler (write() only);
**					- UDP sender: poll() removed, connect()+send(), SO_SNDBUF,
**					  SCHED_FIFO attempt, precise absolute-time pacing;
**					- strict-aliasing safe magic (union);
**					- misc: __FAC__ typo, $LOG texts, help text, printf formats.
**
**	24-JUL-2026	RRL	X.00-03		UDP sender: ECONNREFUSED (ICMP Port Unreachable from
**					a not yet/no more listening receiver) is treated as
**					a transient condition - keep sending at the paced rate
**					instead of the thread termination; the warning is
**					shown under /TRACE only to avoid log flooding.
**					Sender timestamps are removed from payload_t.
**
**	24-JUL-2026	RRL	X.00-04		Receiver: the first valid packet only synchronizes
**					the local sequence base (s_result.seq == 0 is
**					the 'nothing seen yet' sentinel, sender starts
**					from seq 1); a mid-stream receiver start is not
**					accounted as a packet loss anymore.
**					Removed unused l_ts locals in both senders.
**
**	24-JUL-2026	RRL	X.00-05		Receiver: a large sequence number rollback
**					(more than YNETCHK$K_RESYNC packets behind the
**					local base) is treated as a sender restart -
**					the sequence base is resynchronized with a single
**					warning instead of the per-packet 'Old packet'
**					log flood; small rollbacks are still reported
**					as reordered packets.
**
**	24-JUL-2026	RRL	X.00-06		AF_PACKET receiver (s_thread_receiver2): /IFNAME
**					in the receiver mode enables a raw socket bound
**					to the interface with an in-kernel classic BPF
**					filter (IPv4 + UDP + destination port), outgoing
**					packets are skipped by sll_pkttype, the payload
**					offset honors the actual IP header length (IHL);
**					/LIP (when given) is an additional destination IP
**					filter. The sequence/loss accounting is factored
**					out into the common s_rcv_account() routine used
**					by the both receivers. CAP_NET_RAW/root is
**					required for the AF_PACKET receiver.
**					Main statistics: lost is displayed as the
**					per-interval increment along with the cumulative
**					value; the atomics are read once per iteration
**					for a consistent snapshot.
**
**	24-JUL-2026	RRL	X.00-07		SCHED_FIFO is removed (s_set_rt_prio and its
**					calls): with a small /DELTA the paced sender
**					degenerates into a real-time busy loop
**					(clock_nanosleep on an already passed absolute
**					time returns immediately) and starves the whole
**					machine on a single/weak core system.
**
**	24-JUL-2026	RRL	X.00-08		The USAGE chapter is added to the module header:
**					mode selection rules, options reference, launch
**					examples, statistics format, shutdown notes.
**
**	24-JUL-2026	RRL	X.00-09		/TMAC=<aa:bb:cc:dd:ee:ff> CLI/config option:
**					destination MAC (the next hop router/gateway)
**					for the AF_PACKET sender; overrides the compile
**					time default g_tmac, parsed and validated at
**					the configuration validation stage.
**
**	24-JUL-2026	RRL	X.00-10		AF_PACKET sender: when /LIP is omitted the source
**					IP is taken from the primary IPv4 address of the
**					/IFNAME interface (SIOCGIFADDR); the explicit
**					/LIP still takes precedence; an interface with
**					no IPv4 address is a configuration error.
**
**	24-JUL-2026	RRL	X.00-11		All the inline comments are translated into
**					English; no functional changes.
**
**	 1-AUG-2026	RRL	X.01-00		The command line processing is ported from the
**					__util$getparams()/OPTS engine to CLI_ROUTINES
**					(>= X.00-10): the operational mode is now selected
**					by a verb - 'send' or 'receive' - instead of being
**					derived from the /TIP presence; /TIP is declared
**					CLI$M_REQUIRED for 'send'; the port defaults live
**					in the descriptor <defval> fields; /TMAC is
**					validated by the library CLI$K_MAC type (and now
**					accepts the aa-bb-... form as well); /HELP, -h,
**					--help are served by the library __cli$help().
**					The /CONFIG file option is RETIRED - it was a
**					__util$getparams() facility with no CLI_ROUTINES
**					counterpart yet.
**
**	 1-AUG-2026	RRL	X.01-01		The help is served through the HLP_ROUTINES
**					help library API (an LBR$OUTPUT_HELP analog over
**					the yanetchk.hlp source in the classic .HLP
**					format): topic paths are supported - 'yanetchk
**					help send tmac', '<verb> /HELP', /HELP, -h,
**					--help. The library location: $YANETCHK_HLP,
**					then ./yanetchk.hlp, then alongside the binary.
**					When no .HLP source is found the table-driven
**					__cli$help() output is used as the fallback.
**
**	 1-AUG-2026	RRL	X.01-02		The help is delegated to the EXTERNAL HELP package:
**					the yanetchk.md source is compiled into the
**					YANETCHK.HLB library by HELPTRAN, and /HELP
**					fork/execs the 'help' browser against it (found
**					next to this binary, then on the PATH), passing
**					the topic path words as the arguments. The
**					bundled HLP_ROUTINES reader is dropped. The
**					library is located via $YANETCHK_HLP, next to
**					the binary, the build-time YNETCHK$K_HLBDIR and
**					./yanetchk.hlb. When neither the library nor the
**					browser is available the table-driven __cli$help()
**					output is the fallback, so /HELP always answers.
**
**	 3-AUG-2026	RRL	X.01-03		Reorder-aware sequence accounting: the receiver
**					keeps a sliding window (YNETCHK$K_WINSZ seq
**					numbers) of the seen sequence bitmap. A gap is
**					held pending and charged to 'lost' only when it
**					is evicted from the window still unseen, so a
**					late out-of-order arrival that fills the gap is
**					recovered instead of being double counted. New
**					counters exposed in the receiver statistics:
**					reord (reordered, with the deepest displacement)
**					and dup (duplicates). The old flat "Old packet"/
**					"Lost N" logic and the immediate loss charge on
**					a forward gap are retired.
**
**	 3-AUG-2026	RRL	X.01-04		The receiver sequence diagnostics (base sync, gap,
**					reorder, deep reorder, duplicate, sender restart)
**					are signalled VMS style through a message
**					catalogue: each is a condition value (facility
**					YNETCHK$K_FACNO | message number | severity) with
**					an FAO control string, formatted by __util$faol()
**					and emitted as "%YANETCHK-<sev>: IDENT, text" via
**					s_ynetchk$putmsg(), instead of inline printf-style
**					$LOG strings. The FAO parameters are !UQ (the
**					64-bit sequence numbers).
**
**	 3-AUG-2026	RRL	X.01-05		The external HELP browser image name is now
**					configurable: the HELP package may install it
**					under a name other than "help" (e.g. "vhelp", to
**					avoid the shell builtin). /HELP honours the
**					build-time YNETCHK$K_HELPCMD default and the
**					$YANETCHK_HELP override, and tries the known
**					names (vhelp, help) in turn both next to the
**					binary and on the PATH. Verified against the
**					UTILITY$ROUTINES and HELP packages current as of
**					this revision.
**
**	 3-AUG-2026	RRL	X.01-06		Compiles clean under -Wall -Wextra with no
**					suppressions: the pthread start routines go
**					through thin void*-returning trampolines
**					(s_thr_*) instead of an incompatible
**					function-pointer cast, and the interface name
**					is copied with a length-clamped memcpy plus an
**					explicit NUL instead of strncpy (which tripped
**					-Wstringop-truncation on the 254-octet ASCIC).
**
**	 3-AUG-2026	RRL	X.01-07		The receiver statistics line is raised to a
**					WARNING when no packet arrived in the last
**					interval (+0 received) - it flags a silent path
**					or a stopped sender, including the case where a
**					healthy stream stops mid-run. A non-empty
**					interval stays at INFO as before.
**
**	 3-AUG-2026	RRL	X.01-08		The receiver diagnostics use the UTILITY$ROUTINES
**					message facility directly: the catalogue is an
**					EMSG_RECORD array registered with __util$inimsg()
**					and the messages are emitted by $PUTMSG(code, ...),
**					replacing the private s_ynetchk$putmsg()/FAO
**					catalogue of X.01-04. The record text is the full
**					printf-style "%FAC-<sev>-IDENT, ..." envelope that
**					__util$putmsg() expects; starlet.h is no longer
**					needed here. The debug build now reports the real
**					call site in the location tag.
**
**	 3-AUG-2026	RRL	X.01-09		The receiver diagnostics use the FAO variant of
**					message signalling: the catalogue is registered
**					with __util$inimsg() for the code/severity lookup,
**					but each record text is an FAO control string
**					(!UQ) formatted by __util$faol() through
**					s_ynetchk$putmsg(), not the printf __util$putmsg().
**					SEQSYNC is raised to a WARNING: a base (re)sync is
**					an operator-visible event (a mid-run resync flags
**					a discontinuity), so it no longer hides at INFO.
**
**	 3-AUG-2026	RRL	X.01-10		Maximised the use of the UTILITY$ROUTINES package:
**					the receiver diagnostics are signalled by the
**					package's own $PUTMSG()/__util$inimsg() (the
**					private FAO putmsg wrapper and starlet.h are gone,
**					the record text is the printf envelope that
**					__util$putmsg() renders, and the debug build shows
**					the real call site); the DATALEN clamps use $RANGE
**					instead of paired $MAX/$MIN; the per-packet trace
**					uses $IFTRACE (a no-op in Release) instead of a
**					hand-rolled if (g_trace) $LOG; the window bitmap
**					is cleared with __util$bzero. SEQSYNC stays a
**					WARNING.
**
**	 3-AUG-2026	RRL	X.01-11		The receiver diagnostics use the FAO message facility
**					now provided by the package itself: $PUTMSG_FAO()
**					(__util$putmsg_fao/__util$putmsgd_fao, added in
**					UTILITY$ROUTINES V.01-18). The catalogue records are
**					FAO control strings (!UQ) formatted through the
**					starlet FAO service; the debug build shows the real
**					call site. Requires UTILITY$ROUTINES V.01-18 or later.
**
**	 3-AUG-2026	RRL	X.01-12		Fixed a spurious loss count on a mid-stream start (or
**					a restart to a high sequence): the sliding window
**					was seeded with only the first sequence, then as it
**					slid forward it evicted the WINSZ-1 sequences BELOW
**					the seed - which precede the receiver and were never
**					expected - and charged each as a loss, inflating
**					'lost' by up to YNETCHK$K_WINSZ (4096). A win_base
**					floor (the seed of the current session) now guards
**					every eviction: a sequence below win_base is a
**					pre-start phantom and is never counted. Genuine gaps
**					at or above win_base are still charged on eviction.
**
**	 3-AUG-2026	RRL	X.01-13		Adjusted to the released $PUTMSG_FAO() API, which
**					takes an explicit FAO parameter count as its second
**					argument - $PUTMSG_FAO(sts, prmcnt, ...) - so the
**					formatter reads exactly the parameters supplied
**					instead of assuming a fixed maximum. Verified against
**					UTILITY$ROUTINES V.01-18 (the released FAO putmsg)
**					and HELP X-22.
**
**	 3-AUG-2026	RRL	X.01-14		The periodic and closing statistics lines now carry a
**					message IDENT like every other diagnostic: STATS
**					(receiver interval), SENT (sender interval),
**					NOTRAFFIC (idle interval, a WARNING) and FINAL
**					(closing tally). They are catalogue entries emitted
**					by $PUTMSG_FAO(), so each is a full VMS
**					"%YANETCHK-<sev>-IDENT, ..." record that can be
**					filtered by name, instead of the bare
**					"%YANETCHK-<sev>: text" the $LOG path produced.
**
**	 3-AUG-2026	RRL	X.01-15		Adjusted to the $PUTMSG_FAO() of UTILITY$ROUTINES
**					V.01-21, whose macro now both counts its FAO
**					parameters and casts each to fao_prm_t itself: a
**					call is written plainly - $PUTMSG_FAO(sts, a, b) -
**					with neither an explicit count nor per-argument
**					(fao_prm_t) casts. Verified against UTILITY$ROUTINES
**					V.01-21 and HELP X-22.
**
**	 7-AUG-2026	RRL	X.01-16		Added a TPACKET (PACKET_MMAP) TX-ring generator,
**					selected by /RING on 'send' over the AF_PACKET
**					engine (/IFNAME). Frames are written into an
**					mmap'd ring and flushed in batches with one send()
**					for many frames, instead of one syscall per frame:
**					~40x the packet rate of the sendto() path on the
**					same host. Two speed modes share /DELTA - /DELTA=0
**					runs line-rate (fill every free slot, flush,
**					repeat), a non-zero /DELTA keeps its per-frame
**					pacing. /RING without /IFNAME is ignored with a
**					warning.
**
**	 7-AUG-2026	RRL	X.01-17		Adjusted to UTILITY$ROUTINES V.01-24 (StarLet 1.2.2),
**					where __util$putmsg_fao() now supplies the
**					"%<facility>-<sev>-" envelope itself from the
**					catalogue descriptor's facname. s_msgdesc.facname
**					is set to __FAC__ ("YANETCHK") and the leading
**					"%YANETCHK-<sev>-" was removed from every record
**					text (now "IDENT, <body>"); otherwise the envelope
**					printed twice (%YANETCHK-W-%YANETCHK-W-...). The
**					emitted line is unchanged from the operator's view.
**
**	 7-AUG-2026	RRL	X.01-18		/DELTA is now expressed in nanoseconds instead of
**					microseconds: the qualifier value feeds g_delta_ns
**					directly (default 10000000 = 10 ms), and the
**					timespec split uses 1e9. This gives sub-microsecond
**					pacing granularity for the high-rate generators.
**					NOTE: an incompatible change for scripts - a former
**					/DELTA=1000 (1 ms) is now /DELTA=1000000.
**
**	 7-AUG-2026	RRL	X.01-19		The sender start-up now prints the calculated offered
**					load derived from /DELTA and the frame size: the
**					packet rate (pps) and two bandwidths - L2 (the
**					Ethernet frame as built) and wire (L2 + 24 octets
**					of FCS, preamble/SFD and inter-frame gap, i.e. the
**					real link utilisation). At /DELTA=0 the rate is
**					reported as unbounded line rate. Shown for the
**					transmitter only.
**
**	 7-AUG-2026	RRL	X.01-20		The periodic STATS/SENT lines now carry the measured
**					throughput of the elapsed interval: the packet
**					rate (pps) and the wire bandwidth (kbit/s, frame
**					octets + FCS/preamble/IFG). The interval length is
**					measured with CLOCK_MONOTONIC rather than assumed,
**					so a signal-shortened sleep does not skew it. The
**					receiver bandwidth is computed from the octets it
**					actually took off the wire (a new s_result.octets
**					accumulator), so it is correct even when the local
**					/DATALEN differs from the sender's frame; the
**					transmitter uses its own fixed frame size.
**
**	 7-AUG-2026	RRL	X.01-21		A sender-restart resync now resets the session
**					statistics: on the deep-rollback branch the
**					receiver zeroes total, lost, reord, dup, reord_max
**					and octets so the new session is measured from
**					scratch (total no longer exceeds Seq across a
**					restart). A new s_result.epoch is bumped on the
**					resync; the statistics loop rebases its own
**					snapshots when the epoch changes, so the per-interval
**					increments are not driven negative by the reset. A
**					plain large forward gap (a loss, not a restart) does
**					NOT reset - only the deep rollback does.
**
**	 7-AUG-2026	RRL	X.01-22		Added a switch for the per-packet INFO diagnostics
**					SEQGAP (a forward gap held pending) and REORD (a
**					recovered reorder), which flood the log at high
**					rate. The counters and the periodic statistics are
**					unaffected (reord is still tallied), and the
**					WARNING-level events (DUP/DEEPREORD/SEQSYNC/RESTART)
**					always print.
**
**	 7-AUG-2026	RRL	X.01-23		SEQGAP/REORD are now silent by DEFAULT and enabled by
**					the receiver's /VERBOSE qualifier (the earlier
**					/NOVERBOSE opt-out is gone): the quiet log is the
**					common case, /VERBOSE opts into the running
**					per-packet commentary.
**
**	 7-AUG-2026	RRL	X.01-24		Added a TPACKET (PACKET_MMAP) RX-ring receiver,
**					selected by /RING on 'receive' over the AF_PACKET
**					engine (/IFNAME) - the symmetric counterpart of the
**					TX-ring generator. Frames are read from an mmap'd
**					ring instead of one recvfrom() per packet, which
**					was the bottleneck that made the plain receiver drop
**					the bulk of a line-rate stream: against the TX ring
**					on loopback the loss fell from ~83% to ~29% (about
**					5x more packets captured). The BPF filter, the /LIP
**					check and the frame parse are unchanged; /RING needs
**					/IFNAME and is ignored with a warning otherwise.
**
**	 7-AUG-2026	RRL	X.01-25		The /RING receiver was moved from TPACKET_V2 to
**					TPACKET_V3: the RX ring is now block-based rather
**					than frame-based - the kernel packs variable-length
**					frames into 256 KiB blocks (16 of them) and retires
**					a block on fill or after 60 ms, and the thread walks
**					each block's frames by tp_next_offset. This lowers
**					the per-frame overhead further; on loopback the V3
**					receiver sustains ~700 kpps (~1.5 Gbit/s) against
**					the TX-ring generator. The frame parse was factored
**					into s_rx_frame(). The TX ring stays on V2, where
**					the block model brings no benefit.
**
**	 7-AUG-2026	RRL	X.01-26		The TX-ring generator now sets PACKET_QDISC_BYPASS:
**					frames skip the kernel traffic-control (qdisc)
**					layer and go straight to the driver transmit
**					queue, shedding the enqueue/dequeue on the hot send
**					path. It is best-effort - an old kernel or a denied
**					option only warns and the generator proceeds with
**					the qdisc. The gain is small on loopback (cheap
**					qdisc path) but real on a NIC with an active qdisc.
**
**	 7-AUG-2026	RRL	X.01-27		The TX-ring generator no longer aborts on a transient
**					send() error. At line rate on a real NIC send()
**					returns ENOBUFS (errno 105) - or EAGAIN - when the
**					driver TX queue is momentarily full; PACKET_QDISC_
**					BYPASS (X.01-26) makes this more frequent, having no
**					qdisc to absorb the burst. Previously any non-EINTR
**					error logged and broke out of the loop, so a single
**					ENOBUFS stopped the generator dead (rate fell to 0).
**					Now ENOBUFS/EAGAIN wait on POLLOUT for the ring to
**					drain and retry - the unsent slots stay pending - and
**					only a genuinely unexpected errno is fatal.
**
**	13-AUG-2026	RRL	X.01-28		DUP joined SEQGAP/REORD under /VERBOSE: on a path that
**					duplicates heavily the per-packet "Duplicate
**					detected" warning flooded the log, so it is now
**					silent by default and printed only with /VERBOSE.
**					The duplicate counter is still tallied and shown in
**					the periodic STATS and the final summary; the rarer
**					structural warnings (DEEPREORD/SEQSYNC/RESTART) keep
**					printing unconditionally.
**
**	18-AUG-2026	RRL	X.01-28ECO01	ECO01 - header conventions only, no code
**					change: added the AUTHORS block (StarLet Squad and
**					Ruslan R. Laishev, AKA BadAss SysMan) and the
**					CREATION DATE, stamped every MODIFICATION HISTORY
**					entry with the RRL author mark, and set __REV__ to
**					the ECO form derived from __IDENT__.
**
**	12-NOV-2026	CLD	X.01-28ECO02	ECO02 - no functional change: the message catalogue
**					records (YNETCHK$MSGREC) are designated initialisers,
**					which ends the ten -Wmissing-field-initializers
**					warnings on <facname>; the build is clean with
**					YNETCHK$M_STRICT (-Werror). The USAGE default of
**					/DELTA is corrected to 10000000 ns (X.01-18).
**
**	12-NOV-2026	RRL	X.01-28ECO03	ECO03 - no code change: the help library source
**					doc/yanetchk.md (shipped as yanetchk.hlb) shows
**					the output as the program prints it now - the
**					Delta, Calculated load, SENT, STATS, SEQSYNC,
**					SEQGAP/REORD/DUP and RESTART lines, taken from a
**					loopback run; /TRACE shows the per-packet
**					ECONNREFUSED line in a Debug build only; a sender
**					restart is seen only after more than 100000
**					packets and zeroes the counters. __REV__ takes
**					the aa.bb.cc form of the rule (1.28.3).
**--
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<arpa/inet.h>
#include	<sys/socket.h>
#include	<sys/wait.h>
#include	<sys/ioctl.h>
#include	<errno.h>
#include	<ctype.h>
#include	<time.h>
#include	<poll.h>
#include	<signal.h>
#include	<endian.h>
#include	<pthread.h>
#include	<linux/if_packet.h>
#include	<sys/mman.h>
#include	<linux/filter.h>
#include	<net/ethernet.h>
#include	<net/if.h>
#include	<netinet/in.h>
#include	<netinet/ip.h>
#include	<netinet/udp.h>


#define		__FAC__	"YANETCHK"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE		*/
#include	"utility_routines.h"
#include	"starlet.h"
#include	"cli_routines.h"

#if	__has_include("yanetchk_config.h")
#include	"yanetchk_config.h"
#endif




#ifndef $IPv4_BYTES
#define $IPv4_BYTES_FMT "%u.%u.%u.%u"
#define $IPv4_BYTES(addr) \
		(uint8_t) ((addr) & 0xFF), \
		(uint8_t) (((addr) >> 8) & 0xFF),\
		(uint8_t) (((addr) >> 16) & 0xFF),\
		(uint8_t) (((addr) >> 24) & 0xFF)
#endif

#ifndef $IPv4_ADDR
#define $IPv4_ADDR(a,b,c,d) htonl(( \
    ((uint32_t)(a) << 24) | \
    ((uint32_t)(b) << 16) | \
    ((uint32_t)(c) <<  8) | \
    ((uint32_t)(d)      )) \
)
#endif




#define	YNETCHK$K_UDP_PORT	12345					/* Default UDP port number		*/
#define	YNETCHK$K_BUF_SIZE	2048					/* I/O buffer size, octets		*/

#define	YNETCHK$K_TPFRAME	2048					/* TPACKET TX frame slot size, octets	*/
#define	YNETCHK$K_TPBLOCK	(YNETCHK$K_TPFRAME * 64)			/* TX ring block size (64 frames/block)	*/
#define	YNETCHK$K_TPBLOCKS	16					/* Number of blocks in the TX ring	*/
#define	YNETCHK$K_TPFRAMES	((YNETCHK$K_TPBLOCK * YNETCHK$K_TPBLOCKS) / YNETCHK$K_TPFRAME)

/* TPACKET_V3 RX ring: block-based. A block holds many variable-length
 * frames; the kernel retires a block to userspace when it fills or after
 * YNETCHK$K_V3_TOV milliseconds. Block size must be a multiple of the page
 * size; 256 KiB = 64 pages holds a good burst of small frames.		*/
#define	YNETCHK$K_V3_BLOCKSZ	(1u << 18)				/* RX block size: 256 KiB		*/
#define	YNETCHK$K_V3_BLOCKS	16					/* Number of blocks in the RX ring	*/
#define	YNETCHK$K_V3_FRAMESZ	2048					/* Frame size hint (V3 is variable)	*/
#define	YNETCHK$K_V3_FRAMES	((YNETCHK$K_V3_BLOCKSZ * YNETCHK$K_V3_BLOCKS) / YNETCHK$K_V3_FRAMESZ)
#define	YNETCHK$K_V3_TOV	60					/* Block retire timeout, milliseconds	*/
#define	YNETCHK$K_SNDBUF	(1024 * 1024)				/* SO_SNDBUF size, octets		*/
#define	HLB$HELP_MAXWORD	16					/* Topic path words passed to the browser*/

#define	YNETCHK$K_RESYNC	100000					/* Seq rollback deeper than this many
									 * packets is a sender restart - the
									 * receiver resynchronizes its base;
									 * a shallower rollback is a genuine
									 * network reordering			*/

#define	YNETCHK$K_WINLOG	12					/* Reordering window: log2 of its size	*/
#define	YNETCHK$K_WINSZ		(1u << YNETCHK$K_WINLOG)			/* Window size, sequence numbers (4096)	*/
#define	YNETCHK$K_WINMASK	(YNETCHK$K_WINSZ - 1)			/* Index mask for the circular bitmap	*/
#define	YNETCHK$K_WINWORDS	(YNETCHK$K_WINSZ / 64)			/* 64-bit words in the seen bitmap	*/

									/* Full L2 header stack size for the
									 * AF_PACKET sender			*/
#define	YNETCHK$K_HDRSZ		(sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr))

enum	{
	YNETCHK$K_MODE_RCVR = 0,					/* Receiver mode			*/
	YNETCHK$K_MODE_XMIT = 1						/* Sender mode				*/
	};




ASC	g_logfspec = {0},
	g_ifname = {0},							/* Output interface name		*/
	g_tmac_str = {0}						/* Destination MAC as aa:bb:cc:dd:ee:ff	*/
	;

volatile sig_atomic_t	g_exit_flag = 0;				/* Global flag 'all must be stopped'	*/

int	g_trace = 0,							/* A flag to produce extensible logging	*/
	g_verbose = 0,							/* Print the per-packet INFO trace	*/
									/*   (SEQGAP/REORD/DUP); off by deflt	*/
	g_logsize = 0,							/* A size in octets of the log file	*/
	g_mode = 0,							/* See YNETCHK$K_MODE_*			*/
	g_datalen = 128,						/* UDP mode: payload length;
									 * AF_PACKET mode: full frame length	*/
	g_delta_ns = 10*1000*1000					/* An interval between outgoing packets,
									 * nanoseconds (default 10 ms)		*/
	;

uint32_t	g_tip, g_lip;						/* Target/local IPv4 (network order)	*/
uint16_t	g_tpn, g_lpn;						/* Target/local port (host order)	*/
uint8_t		g_tmac[ETH_ALEN] = {0xaa, 0x04, 0x00, 0xDE, 0xC0, 0x01};/* Target MAC for AF_PACKET mode	*/
struct timespec	g_delta_ts = {0, 0};					/* Pacing interval, filled at validation*/
int		g_ifindex = 0;						/* Output IF index for AF_PACKET mode	*/
int		g_ring    = 0;						/* Use the AF_PACKET TPACKET TX ring	*/




/*
 * CLI_ROUTINES command tables. The operational mode is selected by the verb;
 * the port defaults live in the descriptor <defval> fields, /TIP is required
 * for 'send', /TMAC is validated by the library CLI$K_MAC type.
 * The SQ$_ and RQ$_ enums index the tables for the value extraction code.
 */
enum	{
	SQ$_TIP = 0, SQ$_TPN, SQ$_LIP, SQ$_LPN, SQ$_DATALEN, SQ$_DELTA,
	SQ$_IFNAME, SQ$_TMAC, SQ$_RING, SQ$_TRACE, SQ$_LOGFILE, SQ$_LOGSIZE
	};

enum	{
	RQ$_LIP = 0, RQ$_LPN, RQ$_IFNAME, RQ$_RING, RQ$_TRACE, RQ$_VERBOSE, RQ$_LOGFILE, RQ$_LOGSIZE
	};

static CLI_PQDESC	s_send_quals [] = {
	{ .name = {$ASCINI("TIP")},	.type = CLI$K_IPV4, .flag = CLI$M_REQUIRED },
	{ .name = {$ASCINI("TPN")},	.type = CLI$K_NUM,  .defval = {$ASCINI("12345")} },
	{ .name = {$ASCINI("LIP")},	.type = CLI$K_IPV4 },
	{ .name = {$ASCINI("LPN")},	.type = CLI$K_NUM,  .defval = {$ASCINI("12345")} },
	{ .name = {$ASCINI("DATALEN")},	.type = CLI$K_NUM,  .defval = {$ASCINI("128")} },
	{ .name = {$ASCINI("DELTA")},	.type = CLI$K_NUM,  .defval = {$ASCINI("10000000")} },
	{ .name = {$ASCINI("IFNAME")},	.type = CLI$K_FILE },
	{ .name = {$ASCINI("TMAC")},	.type = CLI$K_MAC },
	{ .name = {$ASCINI("RING")},	.type = CLI$K_OPT },
	{ .name = {$ASCINI("TRACE")},	.type = CLI$K_OPT },
	{ .name = {$ASCINI("LOGFILE")},	.type = CLI$K_FILE },
	{ .name = {$ASCINI("LOGSIZE")},	.type = CLI$K_NUM },
	{ .name = {0, ""} }};

static CLI_PQDESC	s_recv_quals [] = {
	{ .name = {$ASCINI("LIP")},	.type = CLI$K_IPV4 },
	{ .name = {$ASCINI("LPN")},	.type = CLI$K_NUM,  .defval = {$ASCINI("12345")} },
	{ .name = {$ASCINI("IFNAME")},	.type = CLI$K_FILE },
	{ .name = {$ASCINI("RING")},	.type = CLI$K_OPT },
	{ .name = {$ASCINI("TRACE")},	.type = CLI$K_OPT },
	{ .name = {$ASCINI("VERBOSE")},	.type = CLI$K_OPT },
	{ .name = {$ASCINI("LOGFILE")},	.type = CLI$K_FILE },
	{ .name = {$ASCINI("LOGSIZE")},	.type = CLI$K_NUM },
	{ .name = {0, ""} }};

static int	s_act_send	(CLI_CTX *a_clictx, void *a_arg);
static int	s_act_recv	(CLI_CTX *a_clictx, void *a_arg);

static CLI_VERB	s_verbs [] = {
	{ .name = {$ASCINI("send")},	.quals = s_send_quals, .act_rtn = s_act_send },
	{ .name = {$ASCINI("receive")},	.quals = s_recv_quals, .act_rtn = s_act_recv },
	{ .name = {0, ""} }};




/* Magic marker; union form is a strict-aliasing safe way to access
 * the same octets as char[8] and as uint64_t				*/
static const union	{
	char		magic	[8];
	uint64_t	magic64;
	} s_magic = { .magic = "StarLet" };


struct payload_t {
	union	{
		char		magic	[8];			/* Magic marker octets			*/
		uint64_t	magic64;			/* Same as single 64-bit word		*/
		};

	uint64_t	seq;					/* Sequence number, starts from 1	*/
} __attribute__((packed));




/*
 * The message catalogue of the YANETCHK facility, signalled through the FAO
 * variant of the UTILITY$ROUTINES message facility: __util$inimsg() registers
 * the catalogue and $PUTMSG_FAO(code, ...) looks a record up by its condition
 * value, formats its text as an FAO control string (via __util$faol()) and
 * emits it with the shared timestamp/PID prefix. As of UTILITY$ROUTINES
 * V.01-24 the "%<facility>-<sev>-" envelope is supplied by putmsg_fao() itself
 * from the descriptor's facname (set to __FAC__ below), so a record text is
 * only "IDENT, <body>" - the leading "%YANETCHK-<sev>-" must NOT be repeated
 * here or it would print twice. Each condition value packs the facility
 * number, the message number and the severity; !UQ carries the 64-bit
 * sequence numbers. Parameters are passed as fao_prm_t.
 */
#define	YNETCHK$K_FACNO		0x596E		/* 'Yn' - the YANETCHK facility number	*/

#define	$YNETCHKSTS(msg, sev)	( $DEFFAC(YNETCHK$K_FACNO) | $DEFMSG(msg) | $DEFSEV(sev) )

enum	{
	YNETCHK$K_MSG_SEQSYNC = 1,			/* Sequence base synchronized		*/
	YNETCHK$K_MSG_SEQGAP,				/* Forward gap, held pending		*/
	YNETCHK$K_MSG_REORD,				/* Reordered packet, recovered		*/
	YNETCHK$K_MSG_DEEPREORD,			/* Reorder below the window		*/
	YNETCHK$K_MSG_DUP,				/* Duplicate packet			*/
	YNETCHK$K_MSG_RESTART,				/* Sender restart, resynchronized	*/
	YNETCHK$K_MSG_STATS,				/* Periodic receiver statistics		*/
	YNETCHK$K_MSG_NOTRAFFIC,			/* No packet arrived in the interval	*/
	YNETCHK$K_MSG_SENT,				/* Periodic sender statistics		*/
	YNETCHK$K_MSG_FINAL				/* Closing tally on shutdown		*/
	};

/* SEQSYNC is a WARNING: a base (re)synchronization means the receiver has
 * (re)aligned to the stream, which is an operator-visible event, not routine
 * info - a mid-run resync in particular flags a discontinuity		*/
#define	YNETCHK$_SEQSYNC	$YNETCHKSTS(YNETCHK$K_MSG_SEQSYNC,	STS$K_WARN)
#define	YNETCHK$_SEQGAP		$YNETCHKSTS(YNETCHK$K_MSG_SEQGAP,	STS$K_INFO)
#define	YNETCHK$_REORD		$YNETCHKSTS(YNETCHK$K_MSG_REORD,	STS$K_INFO)
#define	YNETCHK$_DEEPREORD	$YNETCHKSTS(YNETCHK$K_MSG_DEEPREORD,	STS$K_WARN)
#define	YNETCHK$_DUP		$YNETCHKSTS(YNETCHK$K_MSG_DUP,		STS$K_WARN)
#define	YNETCHK$_RESTART	$YNETCHKSTS(YNETCHK$K_MSG_RESTART,	STS$K_WARN)
#define	YNETCHK$_STATS		$YNETCHKSTS(YNETCHK$K_MSG_STATS,	STS$K_INFO)
#define	YNETCHK$_NOTRAFFIC	$YNETCHKSTS(YNETCHK$K_MSG_NOTRAFFIC,	STS$K_WARN)
#define	YNETCHK$_SENT		$YNETCHKSTS(YNETCHK$K_MSG_SENT,		STS$K_INFO)
#define	YNETCHK$_FINAL		$YNETCHKSTS(YNETCHK$K_MSG_FINAL,	STS$K_INFO)

/*
 * The catalogue records for __util$inimsg()/$PUTMSG_FAO(). The <text> is an
 * FAO control string of the form "IDENT, <body>"; putmsg_fao() prepends the
 * "%<facname>-<sev>-" envelope (see facname below), so the leading facility
 * and severity are deliberately absent here. The ASCIC length octet <textl>
 * is the literal length. Registered once, at start-up; !UQ parameters are
 * passed as sequence numbers. The initialisers are designated: <facname> is
 * stamped by __util$inimsg() from s_msgdesc, <mnemonic> stays NULL (the
 * "IDENT, " of the text names the message).
 */
#define	YNETCHK$MSGREC(code, fao)	{ .sts = (int) (code), .textl = (unsigned char) (sizeof(fao) - 1), .text = fao }

static EMSG_RECORD	s_msgrec [] = {
	YNETCHK$MSGREC(YNETCHK$_SEQSYNC,   "SEQSYNC, Sequence base synchronized, Seq: !UQ"),
	YNETCHK$MSGREC(YNETCHK$_SEQGAP,    "SEQGAP, Seq gap: !UQ -> !UQ (!UQ ahead), pending"),
	YNETCHK$MSGREC(YNETCHK$_REORD,     "REORD, Rcvd Seq: !UQ --- Reordered (!UQ behind), recovered"),
	YNETCHK$MSGREC(YNETCHK$_DEEPREORD, "DEEPREORD, Rcvd Seq: !UQ is !UQ behind the window - deep reorder"),
	YNETCHK$MSGREC(YNETCHK$_DUP,       "DUP, Rcvd Seq: !UQ --- Duplicate detected"),
	YNETCHK$MSGREC(YNETCHK$_RESTART,   "RESTART, High Seq: !UQ --> Rcvd Seq: !UQ --- Sender restart detected, resynchronize"),
	YNETCHK$MSGREC(YNETCHK$_STATS,     "STATS, Rcvd: +!UQ pkts, total: !UQ pkts, lost: +!UQ/!UQ, reord: +!UQ/!UQ (max !UQ), dup: +!UQ/!UQ, Seq: !UQ, delta: !UQ nsec, rate: !UQ pps, !UQ kbit/s (wire)."),
	YNETCHK$MSGREC(YNETCHK$_NOTRAFFIC, "NOTRAFFIC, Rcvd: +0 pkts in the last interval - no traffic received, total: !UQ pkts, lost: !UQ, reord: !UQ (max !UQ), dup: !UQ, Seq: !UQ."),
	YNETCHK$MSGREC(YNETCHK$_SENT,      "SENT, Sent: +!UQ pkts, total: !UQ pkts, Seq: !UQ, rate: !UQ pps, !UQ kbit/s (wire)."),
	YNETCHK$MSGREC(YNETCHK$_FINAL,     "FINAL, total: !UQ pkts, lost: !UQ, reord: !UQ (max !UQ), dup: !UQ, high Seq: !UQ.")
	};

static EMSG_RECORD_DESC	s_msgdesc = {
	.link	= NULL,
	.msgnr	= (unsigned) ($ARRSZ(s_msgrec)),
	.facno	= YNETCHK$K_FACNO,
	.msgrec	= s_msgrec,
	.facname = __FAC__					/* Envelope facility, prefixed by __util$putmsg_fao()	*/
	};




struct result_t {
	_Atomic uint64_t	total,				/* Total packets sent/received		*/
				lost,				/* Confirmed lost (evicted unseen)	*/
				reord,				/* Reordered (arrived below the high	*/
									/*   water mark, not seen before)	*/
				dup,				/* Duplicates (seen twice)		*/
				reord_max,			/* Deepest reorder displacement seen	*/
				seq,				/* High water mark of the sequence	*/
				octets,				/* Total L2 octets sent/received	*/
				epoch,				/* Bumped on every resync - lets the	*/
									/*   stats loop rebase its increments	*/
				delta_ns;			/* Inter-arrival delta, nanoseconds	*/

	/* Receiver thread private state - the reordering window		*/
	struct timespec		last;				/* Last arrival (inter-arrival timing)	*/
	uint64_t		win_hi;				/* Highest seq accepted into the window	*/
	uint64_t		win_base;			/* Lowest real seq of the current session:*/
									/*   evictions below it are pre-start phantom*/
	uint64_t		win_seen [YNETCHK$K_WINWORDS];	/* Bitmap: seq (win_hi-WINSZ+1 .. win_hi)*/
};
static struct result_t	s_result;




/*
 *  DESCRIPTION: Compute the standard Internet (RFC 1071) 16-bit one's
 *		complement checksum over a buffer.
 *
 *  INPUTS:
 *	a_buf:	a buffer to be checksummed
 *	a_len:	a length of the buffer in octets
 *
 *  OUTPUTS:
 *	NONE
 *
 *  RETURNS:
 *	16-bit checksum value (ready to be stored into the header field)
 */
static uint16_t	s_ip_checksum	(const void *a_buf, int a_len)
{
const uint16_t	*l_p = (const uint16_t *) a_buf;
uint32_t	l_sum = 0;

	for ( ; a_len > 1; a_len -= 2 )
		l_sum += *(l_p++);

	if ( a_len )
		l_sum += *((const uint8_t *) l_p);

	l_sum = (l_sum & 0xFFFF) + (l_sum >> 16);
	l_sum = (l_sum & 0xFFFF) + (l_sum >> 16);

	return	(uint16_t) ~l_sum;
}


/*
 *  DESCRIPTION: Common accounting for a validated incoming packet: total
 *		counter, inter-arrival timing, sequence tracking with
 *		loss/duplicate/reorder detection, base synchronization on
 *		the very first packet and on a sender restart (a rollback
 *		deeper than YNETCHK$K_RESYNC packets).
 *		Called from the receiver thread only (s_result.last is
 *		the receiver thread private field).
 *
 *  INPUTS:
 *	a_payload:	validated payload of the incoming packet
 *
 *  OUTPUTS:
 *	s_result is updated
 *
 *  RETURNS:
 *	NONE
 */
/*
 *  DESCRIPTION: Reordering window primitives. The window tracks which of the
 *		last YNETCHK$K_WINSZ sequence numbers, ending at win_hi, have
 *		been seen, in a circular bitmap indexed by (seq & WINMASK).
 *		A sequence is confirmed lost only when it is evicted from the
 *		low end of the window still unseen; a sequence that arrives
 *		below win_hi but is still inside the window and not yet seen
 *		is a reorder, not a loss.
 */
static inline int	s_win_test	(uint64_t a_seq)
{
	return	(s_result.win_seen[(a_seq & YNETCHK$K_WINMASK) >> 6] >> (a_seq & 63)) & 1u;
}

static inline void	s_win_set	(uint64_t a_seq)
{
	s_result.win_seen[(a_seq & YNETCHK$K_WINMASK) >> 6] |= (1ULL << (a_seq & 63));
}

static inline void	s_win_clr	(uint64_t a_seq)
{
	s_result.win_seen[(a_seq & YNETCHK$K_WINMASK) >> 6] &= ~(1ULL << (a_seq & 63));
}


/*
 *  DESCRIPTION: Reset the reordering window so that the only seen sequence
 *		is a_seq (used on the first packet and on a sender restart).
 *
 *  INPUTS:
 *	a_seq:	the sequence number to seed the window with
 *
 *  OUTPUTS:
 *	s_result.win_hi, s_result.win_seen, s_result.seq
 *
 *  RETURNS:
 *	NONE
 */
static void	s_win_reset	(uint64_t a_seq)
{
	__util$bzero (s_result.win_seen, sizeof(s_result.win_seen));
	s_result.win_hi   = a_seq;
	s_result.win_base = a_seq;				/* Nothing below the seed is a real loss	*/
	s_win_set (a_seq);
	s_result.seq = a_seq;
}


/*
 *  DESCRIPTION: Advance the window high water mark to a_seq (a_seq > win_hi),
 *		counting as lost every sequence that leaves the low end of
 *		the window still unseen. The intermediate sequences between
 *		the old and the new high water mark enter the window as
 *		pending (not seen); a_seq itself is marked seen.
 *
 *  INPUTS:
 *	a_seq:	the new high water mark (strictly above win_hi)
 *
 *  OUTPUTS:
 *	s_result.win_hi, s_result.win_seen, s_result.lost, s_result.seq
 *
 *  RETURNS:
 *	NONE
 */
static void	s_win_advance	(uint64_t a_seq)
{
uint64_t	l_e, l_evict, l_lo;

	/* A jump wider than the window evicts the whole current window:
	 * everything not yet seen in it is lost, then reseed		*/
	if ( (a_seq - s_result.win_hi) >= YNETCHK$K_WINSZ )
		{
		l_lo = (s_result.win_hi >= (YNETCHK$K_WINSZ - 1))
			? (s_result.win_hi - (YNETCHK$K_WINSZ - 1)) : 0;

		if ( l_lo < s_result.win_base )				/* Do not blame pre-start phantom seqs	*/
			l_lo = s_result.win_base;

		for ( l_e = l_lo; l_e <= s_result.win_hi; l_e++ )
			if ( !s_win_test(l_e) )
				s_result.lost += 1;

		s_win_reset (a_seq);
		return;
		}

	/* Slide one sequence at a time from win_hi+1 up to a_seq. Each new
	 * position reuses a slot WINSZ older; if that evicted sequence was
	 * a real, expected, still-unseen packet, it is now confirmed lost */
	for ( l_e = s_result.win_hi + 1; l_e <= a_seq; l_e++ )
		{
		l_evict = l_e - YNETCHK$K_WINSZ;			/* Sequence leaving the window	*/

		/* Count a loss only for a sequence that belongs to this session
		 * (>= win_base) and left the window unseen; sequences below the
		 * seed are phantom - they precede a mid-stream start or a restart */
		if ( (l_e >= YNETCHK$K_WINSZ) && (l_evict >= s_result.win_base) && !s_win_test(l_evict) )
			s_result.lost += 1;

		s_win_clr (l_e);					/* Enter as pending (not seen)	*/
		}

	s_win_set (a_seq);						/* The arrival itself is seen	*/
	s_result.win_hi = a_seq;
	s_result.seq = a_seq;
}


/*
 *  DESCRIPTION: Common accounting for a validated incoming packet: total
 *		counter, inter-arrival timing, and reorder-aware sequence
 *		tracking over the sliding window. The cases are, in order:
 *
 *		  seq == 0 base ...... first packet, seed the window;
 *		  seq >  win_hi ...... new high water mark, slide forward
 *					(evicted-unseen sequences -> lost);
 *		  win_hi - seq > RESYNC ... sender restart, reseed;
 *		  seq below window ... a very late reorder (reord++);
 *		  seq in window, seen ... duplicate (dup++);
 *		  seq in window, unseen .. reorder, mark seen (reord++);
 *
 *		Loss is charged only on eviction, so a gap that is filled by
 *		a later out-of-order arrival is never counted as a loss.
 *		Called from the receiver thread only.
 *
 *  INPUTS:
 *	a_payload:	validated payload of the incoming packet
 *
 *  OUTPUTS:
 *	s_result is updated
 *
 *  RETURNS:
 *	NONE
 */
static void	s_rcv_account	(const struct payload_t *a_payload)
{
struct timespec	l_now, l_delta;
uint64_t	l_seq = a_payload->seq, l_hi, l_disp;

	s_result.total += 1;

	clock_gettime(CLOCK_MONOTONIC, &l_now);
	__util$sub_time(&l_now, &s_result.last, &l_delta);
	s_result.last = l_now;
	s_result.delta_ns = ((uint64_t) l_delta.tv_sec) * 1000000000ULL + (uint64_t) l_delta.tv_nsec;

	l_hi = s_result.win_hi;

	/* Case 1: the very first valid packet - seed the window, no loss	*/
	if ( !l_hi && !s_result.seq )
		{
		s_win_reset (l_seq);

		$PUTMSG_FAO(YNETCHK$_SEQSYNC, l_seq);
		return;
		}

	/* Case 2: a new high water mark - slide the window forward		*/
	if ( l_seq > l_hi )
		{
		if ( ((l_seq - l_hi) > 1) && g_verbose )
			$PUTMSG_FAO(YNETCHK$_SEQGAP, l_hi, l_seq, (l_seq - l_hi - 1));

		s_win_advance (l_seq);
		return;
		}

	/* Case 3: a deep rollback - a sender restart, reseed the window	*/
	if ( (l_hi - l_seq) > YNETCHK$K_RESYNC )
		{
		$PUTMSG_FAO(YNETCHK$_RESTART, l_hi, l_seq);

		s_win_reset (l_seq);

		/* Resync: the accumulated counters belong to the finished
		 * session; zero them so the new session is measured from
		 * scratch (this is why total stops exceeding Seq). The epoch
		 * bump tells the statistics loop to rebase its own snapshots.
		 * total becomes 1 - this very packet is the new session's
		 * first; octets is dropped (one packet is immaterial to a rate)*/
		s_result.total     = 1;
		s_result.lost      = 0;
		s_result.reord     = 0;
		s_result.dup       = 0;
		s_result.reord_max = 0;
		s_result.octets    = 0;
		s_result.epoch    += 1;
		return;
		}

	/* The reorder displacement: how far below the high water mark	*/
	l_disp = l_hi - l_seq;

	if ( l_disp > s_result.reord_max )
		s_result.reord_max = l_disp;

	/* Case 4: below the window - too old to deduplicate, but it did
	 * arrive out of order; count it as a (deep) reorder		*/
	if ( l_disp >= YNETCHK$K_WINSZ )
		{
		s_result.reord += 1;

		$PUTMSG_FAO(YNETCHK$_DEEPREORD, l_seq, l_disp);
		return;
		}

	/* Case 5: inside the window, already seen - a duplicate		*/
	if ( s_win_test(l_seq) )
		{
		s_result.dup += 1;

		if ( g_verbose )
			$PUTMSG_FAO(YNETCHK$_DUP, l_seq);
		return;
		}

	/* Case 6: inside the window, not seen - a genuine reorder that
	 * fills a gap; mark it seen so it is NOT later counted as lost	*/
	s_win_set (l_seq);
	s_result.reord += 1;

	if ( g_verbose )
		$PUTMSG_FAO(YNETCHK$_REORD, l_seq, l_disp);
}


/*
 *  DESCRIPTION: Receiver thread. Binds an UDP socket to LIP:LPN, receives
 *		datagrams, validates length and magic, tracks sequence numbers,
 *		accounts lost/duplicated/reordered packets. The very first
 *		valid packet only synchronizes the local sequence base;
 *		a sequence rollback deeper than YNETCHK$K_RESYNC packets is
 *		treated as a sender restart and resynchronizes the base.
 *
 *  INPUTS:
 *	a_arg:	unused
 *
 *  OUTPUTS:
 *	s_result is updated
 *
 *  RETURNS:
 *	EXIT_SUCCESS or condition status from $LOG
 */
static int	s_thread_receiver	(void *a_arg)
{
int	l_sd, l_rc;
struct sockaddr_in	l_sk = {0}, l_sk_rem = {0};
char	l_buf[YNETCHK$K_BUF_SIZE] = {0};
socklen_t	l_sk_len;
struct payload_t	*l_payload = (struct payload_t *) l_buf;
struct pollfd	l_pfd;

	(void) a_arg;

	if ( 0 > (l_sd = socket(AF_INET, SOCK_DGRAM, 0)) )
		return	$LOG(STS$K_ERROR, "socket()->%d, errno: %d", l_sd, errno);

	l_sk.sin_family = AF_INET;
	l_sk.sin_addr.s_addr = g_lip;
	l_sk.sin_port = htons(g_lpn);

	if ( 0 > (l_rc = bind(l_sd, (struct sockaddr *) &l_sk, sizeof(l_sk))) )
		return	close(l_sd),
			$LOG(STS$K_ERROR, "bind()->%d, errno: %d", l_rc, errno);

	l_pfd.fd = l_sd;
	l_pfd.events = POLLIN;

	$LOG(STS$K_INFO, "Start <%s> on: " $IPv4_BYTES_FMT ":%d ...",  __PRETTY_FUNCTION__,
	     $IPv4_BYTES(l_sk.sin_addr.s_addr), ntohs(l_sk.sin_port) );

	for ( ; !g_exit_flag; )
		{
		if ( 0 > (l_rc = poll(&l_pfd, 1, 500)) )
			{
			if ( errno == EINTR )
				continue;

			$LOG(STS$K_ERROR, "poll()->%d, errno: %d", l_rc, errno);
			break;
			}

		if ( !(l_pfd.revents & POLLIN) )
			continue;

		l_sk_len = sizeof(struct sockaddr_in);

		if ( 0 > (l_rc = recvfrom(l_sd, l_buf, sizeof(l_buf), 0, (struct sockaddr *) &l_sk_rem, &l_sk_len)) )
			{
			if ( errno == EINTR )
				continue;

			$LOG(STS$K_ERROR, "recvfrom()->%d, errno: %d", l_rc, errno);
			break;
			}

		/* Validate the incoming datagram: it must carry at least
		 * a full payload_t and the correct magic marker; everything
		 * else is an alien/runt datagram and must be ignored	*/
		if ( l_rc < (int) sizeof(struct payload_t) )
			continue;

		if ( l_payload->magic64 != s_magic.magic64 )
			continue;

		s_result.octets += (uint64_t) l_rc + YNETCHK$K_HDRSZ;	/* UDP payload -> L2: add Eth+IP+UDP	*/
		s_rcv_account(l_payload);
		}

	close(l_sd);
	return	EXIT_SUCCESS;
}


/*
 *  DESCRIPTION: Attach an in-kernel classic BPF filter to the AF_PACKET
 *		socket: accept only non-fragmented IPv4/UDP frames with
 *		the given destination port, drop everything else before
 *		it reaches the userspace.
 *
 *  INPUTS:
 *	a_sd:	AF_PACKET socket descriptor
 *	a_port:	UDP destination port to be accepted (host order)
 *
 *  OUTPUTS:
 *	NONE
 *
 *  RETURNS:
 *	STS$K_SUCCESS or error condition from $LOG
 */
static int	s_attach_bpf	(int a_sd, uint16_t a_port)
{
struct sock_filter	l_flt [] = {
	{ 0x28, 0, 0, 0x0000000c },	/* ldh	[12]			; EtherType			*/
	{ 0x15, 0, 8, 0x00000800 },	/* jeq	#0x0800, L1, DROP	; IPv4 only			*/
	{ 0x30, 0, 0, 0x00000017 },	/* ldb	[23]			; IP protocol			*/
	{ 0x15, 0, 6, 0x00000011 },	/* jeq	#17, L2, DROP		; UDP only			*/
	{ 0x28, 0, 0, 0x00000014 },	/* ldh	[20]			; flags + fragment offset	*/
	{ 0x45, 4, 0, 0x00001fff },	/* jset	#0x1fff, DROP, L3	; drop non-first fragments	*/
	{ 0xb1, 0, 0, 0x0000000e },	/* ldxb	4*([14]&0xf)		; X = IP header length (IHL)	*/
	{ 0x48, 0, 0, 0x00000010 },	/* ldh	[x + 16]		; UDP destination port		*/
	{ 0x15, 0, 1, 0x00000000 },	/* jeq	#port, ACCEPT, DROP	; port is patched at runtime	*/
	{ 0x06, 0, 0, 0x00040000 },	/* ret	#0x40000		; ACCEPT (whole frame)		*/
	{ 0x06, 0, 0, 0x00000000 }	/* ret	#0			; DROP				*/
	};
struct sock_fprog	l_prog = { .len = sizeof(l_flt) / sizeof(l_flt[0]), .filter = l_flt };

	/* Patch the destination port into the comparison instruction */
	l_flt[8].k = a_port;

	if ( 0 > setsockopt(a_sd, SOL_SOCKET, SO_ATTACH_FILTER, &l_prog, sizeof(l_prog)) )
		return	$LOG(STS$K_ERROR, "setsockopt(SO_ATTACH_FILTER), errno: %d", errno);

	return	STS$K_SUCCESS;
}


/*
 *  DESCRIPTION: AF_PACKET receiver thread. Opens a raw socket bound to the
 *		interface given by /IFNAME, attaches the in-kernel BPF filter
 *		(IPv4 + UDP + destination port /LPN), receives full Ethernet
 *		frames, skips locally originated (outgoing) packets, honors
 *		the actual IP header length (IHL) when locating the payload,
 *		optionally filters by the destination IP /LIP, validates
 *		the magic marker and passes the payload to the common
 *		accounting routine.
 *		Requires CAP_NET_RAW or root.
 *
 *  INPUTS:
 *	a_arg:	unused
 *
 *  OUTPUTS:
 *	s_result is updated
 *
 *  RETURNS:
 *	EXIT_SUCCESS or condition status from $LOG
 */
static int	s_thread_receiver2	(void *a_arg)
{
int	l_sd, l_rc, l_off;
struct sockaddr_ll	l_sk_ll = {0}, l_sk_from = {0};
socklen_t	l_sk_len;
char	l_buf[YNETCHK$K_BUF_SIZE];
struct iphdr	*l_ip = (struct iphdr *) (l_buf + sizeof(struct ethhdr));
struct payload_t	*l_payload;
struct pollfd	l_pfd;

	(void) a_arg;

	if ( 0 > (l_sd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) )
		return	$LOG(STS$K_ERROR, "socket(AF_PACKET)->%d, errno: %d", l_sd, errno);

	/* Bind to the interface - otherwise the socket receives frames from ALL interfaces */
	l_sk_ll.sll_family   = AF_PACKET;
	l_sk_ll.sll_protocol = htons(ETH_P_IP);
	l_sk_ll.sll_ifindex  = g_ifindex;

	if ( 0 > (l_rc = bind(l_sd, (struct sockaddr *) &l_sk_ll, sizeof(l_sk_ll))) )
		return	close(l_sd),
			$LOG(STS$K_ERROR, "bind(AF_PACKET)->%d, errno: %d", l_rc, errno);

	/* In-kernel filter: only IPv4/UDP to the /LPN port reaches the userspace */
	if ( !(1 & s_attach_bpf(l_sd, g_lpn)) )
		return	close(l_sd), STS$K_ERROR;

	l_pfd.fd = l_sd;
	l_pfd.events = POLLIN;

	$LOG(STS$K_INFO, "Start <%s> on IF index: %d, UDP port: %d ...",
	     __PRETTY_FUNCTION__, g_ifindex, g_lpn);

	for ( ; !g_exit_flag; )
		{
		if ( 0 > (l_rc = poll(&l_pfd, 1, 500)) )
			{
			if ( errno == EINTR )
				continue;

			$LOG(STS$K_ERROR, "poll()->%d, errno: %d", l_rc, errno);
			break;
			}

		if ( !(l_pfd.revents & POLLIN) )
			continue;

		l_sk_len = sizeof(l_sk_from);

		if ( 0 > (l_rc = recvfrom(l_sd, l_buf, sizeof(l_buf), 0, (struct sockaddr *) &l_sk_from, &l_sk_len)) )
			{
			if ( errno == EINTR )
				continue;

			$LOG(STS$K_ERROR, "recvfrom(AF_PACKET)->%d, errno: %d", l_rc, errno);
			break;
			}

		/* Skip our own outgoing frames - matters when the sender and
		 * the receiver share the same host/interface			*/
		if ( l_sk_from.sll_pkttype == PACKET_OUTGOING )
			continue;

		/* The minimal length: Ethernet + IP (no options) + UDP + payload_t;
		 * the exact IHL-based offset check is below			*/
		if ( l_rc < (int) (YNETCHK$K_HDRSZ + sizeof(struct payload_t)) )
			continue;

		/* BPF has already filtered IPv4/UDP/port, here we only compute
		 * the payload offset honoring the actual IP header length	*/
		l_off = (int) sizeof(struct ethhdr) + (l_ip->ihl * 4) + (int) sizeof(struct udphdr);

		if ( l_rc < (int) (l_off + sizeof(struct payload_t)) )
			continue;

		/* An additional destination IP filter when /LIP is given */
		if ( g_lip && (l_ip->daddr != g_lip) )
			continue;

		l_payload = (struct payload_t *) (l_buf + l_off);

		if ( l_payload->magic64 != s_magic.magic64 )
			continue;

		s_result.octets += (uint64_t) l_rc;			/* AF_PACKET: l_rc is the whole L2 frame	*/
		s_rcv_account(l_payload);
		}

	close(l_sd);
	return	EXIT_SUCCESS;
}


/*
 *  DESCRIPTION: The AF_PACKET receiver over the TPACKET (PACKET_MMAP) RX ring.
 *		Frames are delivered by the kernel into a memory-mapped ring of
 *		slots; the thread walks the slots, and for every one the kernel
 *		has marked TP_STATUS_USER it parses the frame in place and hands
 *		the slot back with TP_STATUS_KERNEL. This removes the per-packet
 *		recvfrom() syscall of s_thread_receiver2() - the bottleneck that
 *		makes the plain receiver drop frames against a TX-ring sender -
 *		so far fewer packets are lost at high rate. poll() blocks only
 *		when the ring is momentarily empty. Requires /IFNAME and
 *		CAP_NET_RAW or root; the same BPF filter and /LIP check apply.
 *
 *  INPUTS:
 *	a_arg:	unused
 *
 *  OUTPUTS:
 *	s_result is updated
 *
 *  RETURNS:
 *	EXIT_SUCCESS, or a condition status from $LOG on a setup error
 */
/*
 *  DESCRIPTION: Parse one captured L2 frame the AF_PACKET way and, if it is a
 *		valid YANETCHK datagram, account it. Shared by the TPACKET_V3
 *		block walk; mirrors the checks of s_thread_receiver2().
 *
 *  INPUTS:
 *	a_frame:	pointer to the Ethernet frame
 *	a_len:		captured length in octets (tp_snaplen)
 *
 *  OUTPUTS:
 *	s_result is updated when the frame is a valid datagram
 *
 *  RETURNS:
 *	NONE
 */
static inline void	s_rx_frame	(const uint8_t *a_frame, int a_len)
{
const struct iphdr	*l_ip = (const struct iphdr *) (a_frame + sizeof(struct ethhdr));
const struct payload_t	*l_payload;
int	l_off;

	if ( a_len < (int) (YNETCHK$K_HDRSZ + sizeof(struct payload_t)) )
		return;

	l_off = (int) sizeof(struct ethhdr) + (l_ip->ihl * 4) + (int) sizeof(struct udphdr);

	if ( a_len < (int) (l_off + sizeof(struct payload_t)) )
		return;

	if ( g_lip && (l_ip->daddr != g_lip) )
		return;

	l_payload = (const struct payload_t *) (a_frame + l_off);

	if ( l_payload->magic64 != s_magic.magic64 )
		return;

	s_result.octets += (uint64_t) a_len;
	s_rcv_account(l_payload);
}


static int	s_thread_receiver3	(void *a_arg)
{
int	l_sd, l_rc, l_ver = TPACKET_V3;
struct sockaddr_ll	l_sk_ll = {0};
struct tpacket_req3	l_req = {0};
struct pollfd	l_pfd;
uint8_t	*l_ring = NULL;
size_t	l_ringsz;
unsigned	l_bi = 0;

	(void) a_arg;

	if ( 0 > (l_sd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) )
		return	$LOG(STS$K_ERROR, "socket(AF_PACKET)->%d, errno: %d", l_sd, errno);

	if ( 0 > (l_rc = setsockopt(l_sd, SOL_PACKET, PACKET_VERSION, &l_ver, sizeof(l_ver))) )
		return	close(l_sd), $LOG(STS$K_ERROR, "setsockopt(PACKET_VERSION V3)->%d, errno: %d", l_rc, errno);

	/* In-kernel filter first, so the ring only fills with our IPv4/UDP/port */
	if ( !(1 & s_attach_bpf(l_sd, g_lpn)) )
		return	close(l_sd), STS$K_ERROR;

	/* Lay out the TPACKET_V3 RX ring: block-based, blocks retired on fill
	 * or after tp_retire_blk_tov milliseconds				*/
	l_req.tp_block_size      = YNETCHK$K_V3_BLOCKSZ;
	l_req.tp_block_nr        = YNETCHK$K_V3_BLOCKS;
	l_req.tp_frame_size      = YNETCHK$K_V3_FRAMESZ;
	l_req.tp_frame_nr        = YNETCHK$K_V3_FRAMES;
	l_req.tp_retire_blk_tov  = YNETCHK$K_V3_TOV;
	l_req.tp_feature_req_word = 0;

	if ( 0 > (l_rc = setsockopt(l_sd, SOL_PACKET, PACKET_RX_RING, &l_req, sizeof(l_req))) )
		return	close(l_sd), $LOG(STS$K_ERROR, "setsockopt(PACKET_RX_RING V3)->%d, errno: %d", l_rc, errno);

	l_ringsz = (size_t) l_req.tp_block_size * l_req.tp_block_nr;

	if ( MAP_FAILED == (l_ring = mmap(NULL, l_ringsz, PROT_READ | PROT_WRITE, MAP_SHARED, l_sd, 0)) )
		return	close(l_sd), $LOG(STS$K_ERROR, "mmap(RX ring V3, %zu)->MAP_FAILED, errno: %d", l_ringsz, errno);

	/* Bind AFTER the ring is set up - binding starts the frame flow	*/
	l_sk_ll.sll_family   = AF_PACKET;
	l_sk_ll.sll_protocol = htons(ETH_P_IP);
	l_sk_ll.sll_ifindex  = g_ifindex;

	if ( 0 > (l_rc = bind(l_sd, (struct sockaddr *) &l_sk_ll, sizeof(l_sk_ll))) )
		return	munmap(l_ring, l_ringsz), close(l_sd),
			$LOG(STS$K_ERROR, "bind(AF_PACKET)->%d, errno: %d", l_rc, errno);

	l_pfd.fd     = l_sd;
	l_pfd.events = POLLIN;

	$LOG(STS$K_INFO, "Start <%s> on IF index: %d, UDP port: %d, RX ring V3: %u blocks x %u KiB ...",
	     __PRETTY_FUNCTION__, g_ifindex, g_lpn, (unsigned) YNETCHK$K_V3_BLOCKS,
	     (unsigned) (YNETCHK$K_V3_BLOCKSZ / 1024));

	/*
	 * TPACKET_V3 is block-based. Each block begins with a
	 * tpacket_block_desc; when its block_status carries TP_STATUS_USER the
	 * block is ours, holding num_pkts frames chained by tp_next_offset,
	 * the first at offset_to_first_pkt. After the walk the block is handed
	 * back with TP_STATUS_KERNEL. poll() waits when no block is ready.
	 */
	for ( ; !g_exit_flag; )
		{
		struct tpacket_block_desc	*l_bd = (struct tpacket_block_desc *) (l_ring + (size_t) l_bi * YNETCHK$K_V3_BLOCKSZ);
		struct tpacket3_hdr		*l_ppd;
		unsigned			l_n, l_num;

		if ( !(__atomic_load_n(&l_bd->hdr.bh1.block_status, __ATOMIC_ACQUIRE) & TP_STATUS_USER) )
			{
			if ( 0 > (l_rc = poll(&l_pfd, 1, 500)) )
				{
				if ( errno == EINTR )
					continue;

				$LOG(STS$K_ERROR, "poll()->%d, errno: %d", l_rc, errno);
				break;
				}
			continue;
			}

		l_num = l_bd->hdr.bh1.num_pkts;
		l_ppd = (struct tpacket3_hdr *) ((uint8_t *) l_bd + l_bd->hdr.bh1.offset_to_first_pkt);

		for ( l_n = 0; l_n < l_num; l_n++ )
			{
			s_rx_frame ((const uint8_t *) l_ppd + l_ppd->tp_mac, (int) l_ppd->tp_snaplen);
			l_ppd = (struct tpacket3_hdr *) ((uint8_t *) l_ppd + l_ppd->tp_next_offset);
			}

		/* Hand the block back to the kernel and advance		*/
		__atomic_store_n(&l_bd->hdr.bh1.block_status, TP_STATUS_KERNEL, __ATOMIC_RELEASE);
		l_bi = (l_bi + 1) % YNETCHK$K_V3_BLOCKS;
		}

	munmap(l_ring, l_ringsz);
	close(l_sd);
	return	EXIT_SUCCESS;
}


/*
 *  DESCRIPTION: Sleep until the next absolute wakeup point computed as
 *		(*a_next_wake + *a_interval) on the CLOCK_MONOTONIC time base.
 *		Absolute-time sleeping eliminates drift accumulation.
 *
 *  INPUTS:
 *	a_next_wake:	previous wakeup point (absolute CLOCK_MONOTONIC time)
 *	a_interval:	pacing interval to be added
 *
 *  OUTPUTS:
 *	a_next_wake:	is advanced to the new wakeup point
 *
 *  RETURNS:
 *	0 on success, -1 on error (other than EINTR)
 */
static int	s_precise_sleep_abs	(struct timespec *a_next_wake, const struct timespec *a_interval)
{
int	l_rc;
struct timespec	l_next_abs;

	l_next_abs.tv_sec  = a_next_wake->tv_sec  + a_interval->tv_sec;
	l_next_abs.tv_nsec = a_next_wake->tv_nsec + a_interval->tv_nsec;

	if ( l_next_abs.tv_nsec >= 1000000000L )
		{
		l_next_abs.tv_sec  += 1;
		l_next_abs.tv_nsec -= 1000000000L;
		}

	/*
	 * clock_nanosleep with TIMER_ABSTIME sleeps until the given absolute time.
	 * If the time has already passed, the routine returns immediately.
	 * CLOCK_MONOTONIC guarantees the sleep is not broken by an NTP time step.
	 */
	l_rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &l_next_abs, NULL);

	/* Advance the next wakeup point for the next loop iteration */
	*a_next_wake = l_next_abs;

	/* Ignore EINTR (a signal interruption), treat it as a successful wakeup */
	if ( (0 != l_rc) && (EINTR != l_rc) )
		return	-1;

	return	0;
}


/*
 *  DESCRIPTION: AF_PACKET sender thread. Forms a complete Ethernet + IPv4 +
 *		UDP frame (source MAC is taken from the output interface via
 *		SIOCGIFHWADDR, IP header checksum is computed, UDP checksum
 *		is zero as permitted for IPv4) and transmits it at the paced
 *		rate directly to the interface given by /IFNAME.
 *		g_datalen is treated as the FULL frame length in octets.
 *
 *  INPUTS:
 *	a_arg:	unused
 *
 *  OUTPUTS:
 *	s_result is updated
 *
 *  RETURNS:
 *	EXIT_SUCCESS or condition status from $LOG
 */
static int	s_thread_sender2	(void *a_arg)
{
int	l_rc, l_sd;
struct sockaddr_ll	l_sk_ll = {0};
char	l_buf[YNETCHK$K_BUF_SIZE] = {0};
struct ethhdr	*l_eth = (struct ethhdr *) l_buf;
struct iphdr	*l_ip  = (struct iphdr *)  (l_buf + sizeof(struct ethhdr));
struct udphdr	*l_udp = (struct udphdr *) (l_buf + sizeof(struct ethhdr) + sizeof(struct iphdr));
struct payload_t	*l_payload = (struct payload_t *) (l_buf + YNETCHK$K_HDRSZ);
struct ifreq	l_ifr = {0};
struct timespec	l_next_wake;
uint16_t	l_iplen, l_udplen;

	(void) a_arg;

	/* AF_PACKET/SOCK_RAW: the kernel transmits the buffer as is, all
	 * the headers starting from Ethernet must be formed manually	*/
	if ( 0 > (l_sd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) )
		return	$LOG(STS$K_ERROR, "socket(AF_PACKET)->%d, errno: %d", l_sd, errno);

	/* Obtain the output interface MAC address for the h_source field */
	{
	int	l_n = (int) $ASCLEN(&g_ifname);
	if ( l_n > (IFNAMSIZ - 1) )
		l_n = IFNAMSIZ - 1;
	memcpy (l_ifr.ifr_name, (char *) $ASCPTR(&g_ifname), l_n);
	l_ifr.ifr_name[l_n] = '\0';
	}

	if ( 0 > (l_rc = ioctl(l_sd, SIOCGIFHWADDR, &l_ifr)) )
		return	close(l_sd),
			$LOG(STS$K_ERROR, "ioctl(SIOCGIFHWADDR, %s)->%d, errno: %d", l_ifr.ifr_name, l_rc, errno);

	/* Set up the link level address structure (sockaddr_ll);
	 * with SOCK_RAW the sll_addr field is NOT used by the kernel
	 * for the addressing, but sll_ifindex is mandatory		*/
	l_sk_ll.sll_family   = AF_PACKET;
	l_sk_ll.sll_protocol = htons(ETH_P_IP);
	l_sk_ll.sll_ifindex  = g_ifindex;
	l_sk_ll.sll_halen    = ETH_ALEN;
	memcpy(l_sk_ll.sll_addr, g_tmac, ETH_ALEN);

	/* The Ethernet header */
	memcpy(l_eth->h_dest, g_tmac, ETH_ALEN);
	memcpy(l_eth->h_source, l_ifr.ifr_hwaddr.sa_data, ETH_ALEN);
	l_eth->h_proto = htons(ETH_P_IP);

	/* The IPv4 header; the lengths are derived from the full frame length g_datalen */
	l_iplen  = (uint16_t) (g_datalen - sizeof(struct ethhdr));
	l_udplen = (uint16_t) (l_iplen - sizeof(struct iphdr));

	l_ip->version  = 4;
	l_ip->ihl      = 5;
	l_ip->tos      = 0;
	l_ip->tot_len  = htons(l_iplen);
	l_ip->id       = 0;
	l_ip->frag_off = htons(IP_DF);
	l_ip->ttl      = 64;
	l_ip->protocol = IPPROTO_UDP;
	l_ip->saddr    = g_lip;
	l_ip->daddr    = g_tip;
	l_ip->check    = 0;
	l_ip->check    = s_ip_checksum(l_ip, sizeof(struct iphdr));

	/* The UDP header; a zero checksum is permitted for IPv4 */
	l_udp->source = htons(g_lpn);
	l_udp->dest   = htons(g_tpn);
	l_udp->len    = htons(l_udplen);
	l_udp->check  = 0;

	/* Initialize the payload (at the YNETCHK$K_HDRSZ offset) */
	l_payload->magic64 = s_magic.magic64;
	l_payload->seq     = 1;

	$LOG(STS$K_INFO, "Start <%s>: IF: %s (index: %d), " $IPv4_BYTES_FMT ":%d -> " $IPv4_BYTES_FMT ":%d,"
	     " dst MAC: %02x:%02x:%02x:%02x:%02x:%02x, frame: %d octets",
		__PRETTY_FUNCTION__, l_ifr.ifr_name, g_ifindex,
		$IPv4_BYTES(g_lip), g_lpn, $IPv4_BYTES(g_tip), g_tpn,
		g_tmac[0], g_tmac[1], g_tmac[2], g_tmac[3], g_tmac[4], g_tmac[5],
		g_datalen);

	clock_gettime(CLOCK_MONOTONIC, &l_next_wake);

	/* The main transmit loop; no poll() is used - a blocking sendto()
	 * itself waits for the socket queue space			*/
	for ( ; !g_exit_flag; )
		{
		if ( g_datalen != (l_rc = sendto(l_sd, l_buf, g_datalen, 0,
						(struct sockaddr *) &l_sk_ll, sizeof(l_sk_ll))) )
			{
			if ( (0 > l_rc) && (errno == EINTR) )
				continue;

			$LOG(STS$K_ERROR, "sendto(AF_PACKET)->%d, errno: %d", l_rc, errno);
			break;
			}

		/* Update the statistics and increment the sequence number */
		s_result.total += 1;
		s_result.seq = (l_payload->seq += 1);

		/* Absolute time pacing, no drift accumulation */
		s_precise_sleep_abs(&l_next_wake, &g_delta_ts);
		}

	close(l_sd);
	return	EXIT_SUCCESS;
}


/*
 *  DESCRIPTION: The AF_PACKET sender over the TPACKET (PACKET_MMAP) TX ring.
 *		A memory-mapped ring of frame slots is shared with the kernel;
 *		the frame (Ethernet/IP/UDP/payload, built once as a template)
 *		is written into a slot, the slot is marked TP_STATUS_SEND_REQUEST,
 *		and a single send() flushes a whole batch of slots - one syscall
 *		for many frames instead of one per frame. Two speed modes:
 *		  g_delta == 0 : line rate - fill every available slot, flush,
 *				 repeat (the batch win of the TX ring);
 *		  g_delta >  0 : paced - one frame per interval, flushed each
 *				 time, so /DELTA keeps its meaning.
 *		Requires /IFNAME (raw frames) and CAP_NET_RAW or root.
 *
 *  INPUTS:
 *	a_arg:	unused
 *
 *  OUTPUTS:
 *	s_result is updated
 *
 *  RETURNS:
 *	EXIT_SUCCESS, or a condition status from $LOG on a setup error
 */
static int	s_thread_sender3	(void *a_arg)
{
int	l_rc, l_sd, l_ver = TPACKET_V2, l_paced;
struct sockaddr_ll	l_sk_ll = {0};
struct ifreq	l_ifr = {0};
struct tpacket_req	l_req = {0};
struct timespec	l_next_wake;
struct pollfd	l_pfd = {0};
uint8_t	l_tmpl [YNETCHK$K_BUF_SIZE] = {0};				/* Frame template, built once	*/
struct ethhdr	*l_eth = (struct ethhdr *) l_tmpl;
struct iphdr	*l_ip  = (struct iphdr *)  (l_tmpl + sizeof(struct ethhdr));
struct udphdr	*l_udp = (struct udphdr *) (l_tmpl + sizeof(struct ethhdr) + sizeof(struct iphdr));
uint8_t	*l_ring = NULL;
size_t	l_ringsz, l_hdroff = TPACKET_ALIGN(sizeof(struct tpacket2_hdr));
uint16_t	l_iplen, l_udplen;
uint64_t	l_seq = 1;
unsigned	l_i = 0;

	(void) a_arg;

	l_paced = (g_delta_ts.tv_sec != 0) || (g_delta_ts.tv_nsec != 0);

	if ( 0 > (l_sd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) )
		return	$LOG(STS$K_ERROR, "socket(AF_PACKET)->%d, errno: %d", l_sd, errno);

	/* Select TPACKET_V2 (fixed-size frame header, 64-bit safe)	*/
	if ( 0 > (l_rc = setsockopt(l_sd, SOL_PACKET, PACKET_VERSION, &l_ver, sizeof(l_ver))) )
		return	close(l_sd), $LOG(STS$K_ERROR, "setsockopt(PACKET_VERSION)->%d, errno: %d", l_rc, errno);

	/* Bypass the kernel qdisc layer - frames go straight to the driver
	 * transmit queue, shedding the traffic-control path on the hot send.
	 * Best-effort: it needs a recent kernel (Linux 3.14+) and the option
	 * may be denied, so a failure only warns and the generator proceeds	*/
	{
	int	l_one = 1;

	if ( 0 > setsockopt(l_sd, SOL_PACKET, PACKET_QDISC_BYPASS, &l_one, sizeof(l_one)) )
		$LOG(STS$K_WARN, "setsockopt(PACKET_QDISC_BYPASS) unavailable, errno: %d - continuing with qdisc", errno);
	}

	/* Lay out the TX ring: TPBLOCKS blocks, TPFRAME-sized slots	*/
	l_req.tp_block_size = YNETCHK$K_TPBLOCK;
	l_req.tp_block_nr   = YNETCHK$K_TPBLOCKS;
	l_req.tp_frame_size = YNETCHK$K_TPFRAME;
	l_req.tp_frame_nr   = YNETCHK$K_TPFRAMES;

	if ( 0 > (l_rc = setsockopt(l_sd, SOL_PACKET, PACKET_TX_RING, &l_req, sizeof(l_req))) )
		return	close(l_sd), $LOG(STS$K_ERROR, "setsockopt(PACKET_TX_RING)->%d, errno: %d", l_rc, errno);

	l_ringsz = (size_t) l_req.tp_block_size * l_req.tp_block_nr;

	if ( MAP_FAILED == (l_ring = mmap(NULL, l_ringsz, PROT_READ | PROT_WRITE, MAP_SHARED, l_sd, 0)) )
		return	close(l_sd), $LOG(STS$K_ERROR, "mmap(TX ring, %zu)->MAP_FAILED, errno: %d", l_ringsz, errno);

	/* The output interface MAC for h_source */
	{
	int	l_n = (int) $ASCLEN(&g_ifname);
	if ( l_n > (IFNAMSIZ - 1) )
		l_n = IFNAMSIZ - 1;
	memcpy (l_ifr.ifr_name, (char *) $ASCPTR(&g_ifname), l_n);
	l_ifr.ifr_name[l_n] = '\0';
	}

	if ( 0 > (l_rc = ioctl(l_sd, SIOCGIFHWADDR, &l_ifr)) )
		return	munmap(l_ring, l_ringsz), close(l_sd),
			$LOG(STS$K_ERROR, "ioctl(SIOCGIFHWADDR, %s)->%d, errno: %d", l_ifr.ifr_name, l_rc, errno);

	/* Bind the ring socket to the output interface */
	l_sk_ll.sll_family   = AF_PACKET;
	l_sk_ll.sll_protocol = htons(ETH_P_IP);
	l_sk_ll.sll_ifindex  = g_ifindex;
	l_sk_ll.sll_halen    = ETH_ALEN;
	memcpy(l_sk_ll.sll_addr, g_tmac, ETH_ALEN);

	if ( 0 > (l_rc = bind(l_sd, (struct sockaddr *) &l_sk_ll, sizeof(l_sk_ll))) )
		return	munmap(l_ring, l_ringsz), close(l_sd),
			$LOG(STS$K_ERROR, "bind(AF_PACKET)->%d, errno: %d", l_rc, errno);

	/* Build the frame template once (only the sequence changes per frame) */
	memcpy(l_eth->h_dest, g_tmac, ETH_ALEN);
	memcpy(l_eth->h_source, l_ifr.ifr_hwaddr.sa_data, ETH_ALEN);
	l_eth->h_proto = htons(ETH_P_IP);

	l_iplen  = (uint16_t) (g_datalen - sizeof(struct ethhdr));
	l_udplen = (uint16_t) (l_iplen - sizeof(struct iphdr));

	l_ip->version  = 4;
	l_ip->ihl      = 5;
	l_ip->tos      = 0;
	l_ip->tot_len  = htons(l_iplen);
	l_ip->id       = 0;
	l_ip->frag_off = htons(IP_DF);
	l_ip->ttl      = 64;
	l_ip->protocol = IPPROTO_UDP;
	l_ip->saddr    = g_lip;
	l_ip->daddr    = g_tip;
	l_ip->check    = 0;
	l_ip->check    = s_ip_checksum(l_ip, sizeof(struct iphdr));

	l_udp->source = htons(g_lpn);
	l_udp->dest   = htons(g_tpn);
	l_udp->len    = htons(l_udplen);
	l_udp->check  = 0;

	((struct payload_t *) (l_tmpl + YNETCHK$K_HDRSZ))->magic64 = s_magic.magic64;

	$LOG(STS$K_INFO, "Start <%s>: IF: %s (index: %d), " $IPv4_BYTES_FMT ":%d -> " $IPv4_BYTES_FMT ":%d,"
	     " frame: %d octets, TX ring: %u frames, mode: %s",
		__PRETTY_FUNCTION__, l_ifr.ifr_name, g_ifindex,
		$IPv4_BYTES(g_lip), g_lpn, $IPv4_BYTES(g_tip), g_tpn,
		g_datalen, (unsigned) YNETCHK$K_TPFRAMES, l_paced ? "paced" : "line-rate");

	clock_gettime(CLOCK_MONOTONIC, &l_next_wake);

	l_pfd.fd     = l_sd;
	l_pfd.events = POLLOUT;

	/*
	 * The transmit loop. A frame slot is a tpacket2_hdr followed, at
	 * l_hdroff, by the frame itself. A slot with tp_status ==
	 * TP_STATUS_AVAILABLE is ours to fill; setting TP_STATUS_SEND_REQUEST
	 * hands it to the kernel, and send() flushes every requested slot.
	 */
	for ( ; !g_exit_flag; )
		{
		struct tpacket2_hdr	*l_hdr = (struct tpacket2_hdr *) (l_ring + (size_t) l_i * YNETCHK$K_TPFRAME);
		uint8_t			*l_data;
		int			l_filled = 0;

		/* Fill one slot (paced) or every currently free slot (line rate) */
		do	{
			if ( __atomic_load_n(&l_hdr->tp_status, __ATOMIC_ACQUIRE) != TP_STATUS_AVAILABLE )
				break;					/* Ring full - go flush what we have	*/

			l_data = (uint8_t *) l_hdr + l_hdroff;
			memcpy(l_data, l_tmpl, g_datalen);
			((struct payload_t *) (l_data + YNETCHK$K_HDRSZ))->seq = l_seq;

			l_hdr->tp_len     = g_datalen;
			l_hdr->tp_snaplen = g_datalen;
			__atomic_store_n(&l_hdr->tp_status, TP_STATUS_SEND_REQUEST, __ATOMIC_RELEASE);

			l_seq          += 1;
			l_filled       += 1;
			s_result.total += 1;
			s_result.seq    = l_seq - 1;

			l_i = (l_i + 1) % YNETCHK$K_TPFRAMES;
			l_hdr = (struct tpacket2_hdr *) (l_ring + (size_t) l_i * YNETCHK$K_TPFRAME);
			}
		while ( !l_paced && !g_exit_flag );

		/* Flush the batch: kick the kernel to transmit every requested slot */
		if ( l_filled )
			{
			if ( 0 > (l_rc = send(l_sd, NULL, 0, 0)) )
				{
				if ( errno == EINTR )
					continue;

				/* ENOBUFS/EAGAIN at line rate is transient: the driver
				 * TX queue is momentarily full (more likely with
				 * PACKET_QDISC_BYPASS, which has no qdisc to buffer).
				 * The unsent slots stay TP_STATUS_SEND_REQUEST, so wait
				 * for the ring to drain and let the next send() retry -
				 * this is NOT a fatal condition			*/
				if ( (errno == ENOBUFS) || (errno == EAGAIN) )
					{
					poll(&l_pfd, 1, 100);
					continue;
					}

				$LOG(STS$K_ERROR, "send(TX ring)->%d, errno: %d", l_rc, errno);
				break;
				}
			}
		else if ( !l_paced )
			{
			/* Ring momentarily full and nothing free to fill: block on
			 * POLLOUT until the kernel drains a slot, then retry	*/
			if ( 0 > (l_rc = send(l_sd, NULL, 0, 0)) )
				{
				if ( errno == EINTR )
					continue;

				if ( (errno == ENOBUFS) || (errno == EAGAIN) )
					{
					poll(&l_pfd, 1, 100);
					continue;
					}

				$LOG(STS$K_ERROR, "send(TX ring drain)->%d, errno: %d", l_rc, errno);
				break;
				}
			}

		/* Absolute-time pacing between frames when /DELTA is non-zero */
		if ( l_paced )
			s_precise_sleep_abs(&l_next_wake, &g_delta_ts);
		}

	/* Drain: let the kernel finish transmitting the requested slots */
	send(l_sd, NULL, 0, 0);

	munmap(l_ring, l_ringsz);
	close(l_sd);
	return	EXIT_SUCCESS;
}


/*
 *  DESCRIPTION: UDP sender thread. Sends paced datagrams with incrementing
 *		sequence numbers to TIP:TPN.
 *		connect()+send() is used instead of sendto() to let the kernel
 *		cache the route; no poll() before send - a blocking send()
 *		itself waits for the socket queue space.
 *
 *  INPUTS:
 *	a_arg:	unused
 *
 *  OUTPUTS:
 *	s_result is updated
 *
 *  RETURNS:
 *	EXIT_SUCCESS or condition status from $LOG
 */
static int	s_thread_sender	(void *a_arg)
{
int	l_rc, l_sd, l_sndbuf = YNETCHK$K_SNDBUF;
struct sockaddr_in	l_sk = {0};
char	l_buf[YNETCHK$K_BUF_SIZE] = {0};
struct payload_t	*l_payload = (struct payload_t *) l_buf;
struct timespec	l_next_wake;

	(void) a_arg;

	if ( 0 > (l_sd = socket(AF_INET, SOCK_DGRAM, 0)) )
		return	$LOG(STS$K_ERROR, "socket()->%d, errno: %d", l_sd, errno);

	if ( 0 > setsockopt(l_sd, SOL_SOCKET, SO_SNDBUF, &l_sndbuf, sizeof(l_sndbuf)) )
		$LOG(STS$K_WARN, "setsockopt(SO_SNDBUF, %d), errno: %d", l_sndbuf, errno);

	l_sk.sin_family      = AF_INET;
	l_sk.sin_addr.s_addr = g_tip;
	l_sk.sin_port        = htons(g_tpn);

	/* connect() on an UDP socket caches the route and the destination
	 * in the kernel - saves the route lookup and the sockaddr copying
	 * on the every packet						*/
	if ( 0 > (l_rc = connect(l_sd, (struct sockaddr *) &l_sk, sizeof(l_sk))) )
		return	close(l_sd),
			$LOG(STS$K_ERROR, "connect()->%d, errno: %d", l_rc, errno);

	l_payload->magic64 = s_magic.magic64;
	l_payload->seq = 1;

	$LOG(STS$K_INFO, "Start <%s> to: " $IPv4_BYTES_FMT ":%d ...",  __PRETTY_FUNCTION__,
	     $IPv4_BYTES(l_sk.sin_addr.s_addr), ntohs(l_sk.sin_port) );

	clock_gettime(CLOCK_MONOTONIC, &l_next_wake);

	for ( ; !g_exit_flag; )
		{
		if ( g_datalen != (l_rc = send(l_sd, l_buf, g_datalen, 0)) )
			{
			if ( (0 > l_rc) && (errno == EINTR) )
				continue;

			/* ECONNREFUSED - ICMP Port Unreachable from the receiver:
			 * a transient condition (the receiver is not yet/no
			 * more listening), keep sending at the paced rate;
			 * log under /TRACE only - otherwise it floods with
			 * one message per packet			*/
			if ( (0 > l_rc) && (errno == ECONNREFUSED) )
				{
				$IFTRACE(g_trace, "send()->%d, errno: %d (ICMP Port Unreachable, receiver is not listening)", l_rc, errno);

				s_precise_sleep_abs(&l_next_wake, &g_delta_ts);
				continue;
				}

			$LOG(STS$K_ERROR, "send()->%d, errno: %d", l_rc, errno);
			break;
			}

		s_result.total += 1;
		s_result.seq = (l_payload->seq += 1);

		s_precise_sleep_abs(&l_next_wake, &g_delta_ts);
		}

	close(l_sd);
	return	EXIT_SUCCESS;
}


/*
 *  DESCRIPTION: Signals handler; async-signal-safe (write()/_exit() only).
 *		First SIGTERM/SIGINT/SIGQUIT sets the exit flag, a repeated
 *		signal terminates the process immediately.
 *
 *  INPUTS:
 *	a_signo:	signal number
 *
 *  OUTPUTS:
 *	g_exit_flag is set
 *
 *  RETURNS:
 *	NONE
 */
static	void	s_sig_handler	(int a_signo)
{
static const char	l_msg_exit [] = "Exit flag has been set, exiting ...\n",
			l_msg_set  [] = "Got termination signal, set exit_flag!\n",
			l_msg_othr [] = "Got unexpected signal, exiting ...\n";
ssize_t	l_rc;

	if ( g_exit_flag )
		{
		l_rc = write(STDOUT_FILENO, l_msg_exit, sizeof(l_msg_exit) - 1);
		(void) l_rc;

		_exit(a_signo);
		}

	if ( (a_signo == SIGTERM) || (a_signo == SIGINT) || (a_signo == SIGQUIT) )
		{
		l_rc = write(STDOUT_FILENO, l_msg_set, sizeof(l_msg_set) - 1);
		(void) l_rc;

		g_exit_flag = 1;
		return;
		}

	l_rc = write(STDOUT_FILENO, l_msg_othr, sizeof(l_msg_othr) - 1);
	(void) l_rc;

	_exit(a_signo);
}


/*
 *  DESCRIPTION: Establish handlers for the termination signals,
 *		ignore SIGPIPE.
 *
 *  INPUTS:
 *	NONE
 *
 *  OUTPUTS:
 *	NONE
 *
 *  RETURNS:
 *	NONE
 */
static void	s_init_sig_handler	(void)
{
const int	l_siglist [] = {SIGTERM, SIGINT, SIGUSR1, SIGQUIT, 0};
int	l_i;

	/*
	 * Establishing a signals handler
	 */
	signal(SIGPIPE, SIG_IGN);

	for ( l_i = 0; l_siglist[l_i]; l_i++ )
		{
		if ( (signal(l_siglist[l_i], s_sig_handler)) == SIG_ERR )
			fprintf(stdout, "Error establishing handler for signal %d/%#x, error=%d",
				l_siglist[l_i], l_siglist[l_i], errno);
		}
}


/*
 *  DESCRIPTION: Retrieve an integer qualifier value from the parse context;
 *		the declared <defval> (when any) is served by the library for
 *		an absent qualifier. The target is left intact when the
 *		qualifier is absent and has no default.
 *
 *  INPUTS:
 *	a_clictx:	the CLI parse context
 *	a_pq:		the qualifier descriptor
 *
 *  OUTPUTS:
 *	a_val:		receives the converted value
 *
 *  RETURNS:
 *	STS$K_SUCCESS or STS$K_WARN (absent, no default)
 */
static int	s_qval_int	(CLI_CTX *a_clictx, CLI_PQDESC *a_pq, int *a_val)
{
ASC	l_val = {0};

	if ( !(1 & __cli$present(a_clictx, a_pq, NULL)) && !$ASCLEN(&a_pq->defval) )
		return	STS$K_WARN;

	if ( !(1 & __cli$get_value(a_clictx, a_pq, &l_val)) )
		return	STS$K_WARN;

	*a_val = (int) strtol((char *) $ASCPTR(&l_val), NULL, 0);

	return	STS$K_SUCCESS;
}


/*
 *  DESCRIPTION: Retrieve an IPv4 qualifier value (network byte order);
 *		the string form has been validated by the library CLI$K_IPV4
 *		type check.
 *
 *  INPUTS:
 *	a_clictx:	the CLI parse context
 *	a_pq:		the qualifier descriptor
 *
 *  OUTPUTS:
 *	a_val:		receives the address in the network byte order
 *
 *  RETURNS:
 *	STS$K_SUCCESS or STS$K_WARN (absent)
 */
static int	s_qval_ip4	(CLI_CTX *a_clictx, CLI_PQDESC *a_pq, uint32_t *a_val)
{
ASC	l_val = {0};
struct in_addr	l_ia;

	if ( !(1 & __cli$present(a_clictx, a_pq, NULL)) )
		return	STS$K_WARN;

	if ( !(1 & __cli$get_value(a_clictx, a_pq, &l_val)) )
		return	STS$K_WARN;

	if ( 1 != inet_pton(AF_INET, (char *) $ASCPTR(&l_val), &l_ia) )
		return	STS$K_WARN;

	*a_val = l_ia.s_addr;

	return	STS$K_SUCCESS;
}


/*
 *  DESCRIPTION: Retrieve a string qualifier value as an ASC copy.
 *
 *  INPUTS:
 *	a_clictx:	the CLI parse context
 *	a_pq:		the qualifier descriptor
 *
 *  OUTPUTS:
 *	a_val:		receives a copy of the value
 *
 *  RETURNS:
 *	STS$K_SUCCESS or STS$K_WARN (absent)
 */
static int	s_qval_asc	(CLI_CTX *a_clictx, CLI_PQDESC *a_pq, ASC *a_val)
{
	if ( !(1 & __cli$present(a_clictx, a_pq, NULL)) )
		return	STS$K_WARN;

	return	__cli$get_value(a_clictx, a_pq, a_val);
}


/*
 *  DESCRIPTION: The action routine of the 'send' verb - sets the sender
 *		mode and moves the qualifier values into the configuration
 *		globals.
 *
 *  INPUTS:
 *	a_clictx:	the CLI parse context
 *	a_arg:		unused (the verb's act_arg)
 *
 *  OUTPUTS:
 *	g_mode, g_tip, g_tpn, g_lip, g_lpn, g_datalen, g_delta_ns,
 *	g_ifname, g_tmac_str, g_trace, g_logfspec, g_logsize
 *
 *  RETURNS:
 *	STS$K_SUCCESS
 */
static int	s_act_send	(CLI_CTX *a_clictx, void *a_arg)
{
int	l_tmp;

	(void) a_arg;

	g_mode = YNETCHK$K_MODE_XMIT;

	s_qval_ip4 (a_clictx, &s_send_quals[SQ$_TIP], &g_tip);
	s_qval_ip4 (a_clictx, &s_send_quals[SQ$_LIP], &g_lip);

	if ( 1 & s_qval_int(a_clictx, &s_send_quals[SQ$_TPN], &l_tmp) )
		g_tpn = (uint16_t) l_tmp;
	if ( 1 & s_qval_int(a_clictx, &s_send_quals[SQ$_LPN], &l_tmp) )
		g_lpn = (uint16_t) l_tmp;

	s_qval_int (a_clictx, &s_send_quals[SQ$_DATALEN], &g_datalen);
	s_qval_int (a_clictx, &s_send_quals[SQ$_DELTA], &g_delta_ns);
	s_qval_int (a_clictx, &s_send_quals[SQ$_LOGSIZE], &g_logsize);

	s_qval_asc (a_clictx, &s_send_quals[SQ$_IFNAME], &g_ifname);
	s_qval_asc (a_clictx, &s_send_quals[SQ$_TMAC], &g_tmac_str);
	s_qval_asc (a_clictx, &s_send_quals[SQ$_LOGFILE], &g_logfspec);

	g_trace = (1 & __cli$present(a_clictx, &s_send_quals[SQ$_TRACE], NULL));
	g_ring  = (1 & __cli$present(a_clictx, &s_send_quals[SQ$_RING], NULL));

	return	STS$K_SUCCESS;
}


/*
 *  DESCRIPTION: The action routine of the 'receive' verb - sets the receiver
 *		mode and moves the qualifier values into the configuration
 *		globals.
 *
 *  INPUTS:
 *	a_clictx:	the CLI parse context
 *	a_arg:		unused (the verb's act_arg)
 *
 *  OUTPUTS:
 *	g_mode, g_lip, g_lpn, g_ifname, g_trace, g_logfspec, g_logsize
 *
 *  RETURNS:
 *	STS$K_SUCCESS
 */
static int	s_act_recv	(CLI_CTX *a_clictx, void *a_arg)
{
int	l_tmp;

	(void) a_arg;

	g_mode = YNETCHK$K_MODE_RCVR;

	s_qval_ip4 (a_clictx, &s_recv_quals[RQ$_LIP], &g_lip);

	if ( 1 & s_qval_int(a_clictx, &s_recv_quals[RQ$_LPN], &l_tmp) )
		g_lpn = (uint16_t) l_tmp;

	s_qval_int (a_clictx, &s_recv_quals[RQ$_LOGSIZE], &g_logsize);

	s_qval_asc (a_clictx, &s_recv_quals[RQ$_IFNAME], &g_ifname);
	s_qval_asc (a_clictx, &s_recv_quals[RQ$_LOGFILE], &g_logfspec);

	g_trace = (1 & __cli$present(a_clictx, &s_recv_quals[RQ$_TRACE], NULL));
	g_verbose = (1 & __cli$present(a_clictx, &s_recv_quals[RQ$_VERBOSE], NULL));
	g_ring  = (1 & __cli$present(a_clictx, &s_recv_quals[RQ$_RING], NULL));

	return	STS$K_SUCCESS;
}


/*
 *  DESCRIPTION: Test whether a command line token is a help request:
 *		/HELP, -HELP, --help or the short -h/--h form; a switch
 *		prefix is mandatory (a bare 'help' word is handled by the
 *		caller as the HELP pseudo-verb).
 *
 *  INPUTS:
 *	a_arg:	the raw command line token
 *
 *  OUTPUTS:
 *	NONE
 *
 *  RETURNS:
 *	non-zero when the token requests help
 */
static int	s_is_help_switch	(const char *a_arg)
{
const char	*l_p = a_arg;

	if ( (*l_p != '-') && (*l_p != '/') )
		return	0;

	l_p++;
	if ( *l_p == '-' )
		l_p++;

	if ( ((l_p[0] == 'h') || (l_p[0] == 'H')) && (l_p[1] == '\0') )
		return	1;

	return	(0 == strncasecmp(l_p, "HELP", 4)) && (l_p[4] == '\0');
}


/*
 *  DESCRIPTION: Serve a help request through the HLP_ROUTINES help library
 *		(the LBR$OUTPUT_HELP analog). The .HLP source is looked up
 *		in: $YANETCHK_HLP, ./yanetchk.hlp, the directory of the
 *		binary. When no source is found, the table driven
 *		__cli$help() output is used as the fallback.
 *
 *  INPUTS:
 *	a_keys:	the space separated topic path, e.g. "SEND TMAC"; may be
 *		empty for the top level overview
 *	a_argv0:the argv[0] of the process (to derive the binary directory)
 *
 *  OUTPUTS:
 *	NONE
 *
 *  RETURNS:
 *	NONE (the help output is a best effort service)
 */
/*
 *  DESCRIPTION: Locate the YANETCHK.HLB help library: the $YANETCHK_HLP
 *		environment variable, then next to the binary (from argv[0]),
 *		then the system help directory the product was built with.
 *
 *  INPUTS:
 *	a_argv0:	the program invocation path (argv[0])
 *
 *  OUTPUTS:
 *	a_fspec:	receives the located library specification
 *	a_fspec_sz:	size of the <a_fspec> buffer, octets
 *
 *  RETURNS:
 *	STS$K_SUCCESS or STS$K_WARN (not found)
 */
static int	s_hlb_locate	(const char *a_argv0, char *a_fspec, size_t a_fspec_sz)
{
const char	*l_env, *l_slash;

	if ( (l_env = getenv("YANETCHK_HLP")) && !access(l_env, R_OK) )
		{
		snprintf (a_fspec, a_fspec_sz, "%s", l_env);
		return	STS$K_SUCCESS;
		}

	if ( a_argv0 && (l_slash = strrchr(a_argv0, '/')) )
		{
		snprintf (a_fspec, a_fspec_sz, "%.*s/yanetchk.hlb",
			  (int) (l_slash - a_argv0), a_argv0);

		if ( !access(a_fspec, R_OK) )
			return	STS$K_SUCCESS;
		}

#ifdef	YNETCHK$K_HLBDIR
	snprintf (a_fspec, a_fspec_sz, "%s/yanetchk.hlb", YNETCHK$K_HLBDIR);
	if ( !access(a_fspec, R_OK) )
		return	STS$K_SUCCESS;
#endif

	if ( !access("./yanetchk.hlb", R_OK) )
		{
		snprintf (a_fspec, a_fspec_sz, "./yanetchk.hlb");
		return	STS$K_SUCCESS;
		}

	return	STS$K_WARN;
}


/*
 *  DESCRIPTION: Serve a help request through the EXTERNAL help browser: run
 *		the 'help' utility of the HELP package against the located
 *		YANETCHK.HLB library, passing the topic path words as the
 *		arguments. The browser is looked up next to this binary
 *		first, then on the PATH (execvp). When either the library or
 *		the browser is unavailable, the table driven __cli$help()
 *		output is used as the fallback, so /HELP always answers.
 *
 *  INPUTS:
 *	a_keys:		the space separated topic path, e.g. "SEND TMAC";
 *			may be empty for the top level overview
 *	a_argv0:	the argv[0] of the process (to locate the library
 *			and the sibling browser)
 *
 *  OUTPUTS:
 *	NONE
 *
 *  RETURNS:
 *	NONE (the help output is a best effort service)
 */
static void	s_serve_help	(const char *a_keys, const char *a_argv0)
{
CLI_VERB	*l_v;
char	l_fspec [512], l_libarg [512 + 16], l_sib [512], l_keys [256];
char	*l_argv [8 + HLB$HELP_MAXWORD], *l_tok, *l_save;
const char	*l_slash;
int	l_argc = 0;
pid_t	l_pid;

	/* Without a library there is nothing for the browser to read - fall
	 * straight through to the table driven summary			*/
	if ( !(1 & s_hlb_locate(a_argv0, l_fspec, sizeof(l_fspec))) )
		goto	fallback;

	/* Fork/exec the external browser; the parent waits for it	*/
	if ( 0 == (l_pid = fork()) )
		{
		/* The browser image name. The HELP package may install it
		 * under a name other than "help" (e.g. "vhelp", to dodge the
		 * shell builtin); honour the build-time default and a runtime
		 * override, then try the known names in turn, both next to
		 * this binary and on the PATH.				*/
		const char	*l_names [4];
		int		l_nn = 0, l_i;
		const char	*l_env = getenv("YANETCHK_HELP");

#ifdef	YNETCHK$K_HELPCMD
		const char	*l_dflt = YNETCHK$K_HELPCMD;
#else
		const char	*l_dflt = "vhelp";
#endif
		if ( l_env && *l_env )
			l_names[l_nn++] = l_env;
		l_names[l_nn++] = l_dflt;
		l_names[l_nn++] = "vhelp";
		l_names[l_nn++] = "help";

		snprintf (l_libarg, sizeof(l_libarg), "/library=%s", l_fspec);

		l_argv [l_argc++] = (char *) l_names[0];			/* argv[0], reset per try below	*/
		l_argv [l_argc++] = l_libarg;

		/* Split the topic path into separate argv words		*/
		snprintf (l_keys, sizeof(l_keys), "%s", a_keys ? a_keys : "");

		for ( l_tok = strtok_r(l_keys, " ", &l_save);
		      l_tok && (l_argc < (int) ($ARRSZ(l_argv) - 1));
		      l_tok = strtok_r(NULL, " ", &l_save) )
			l_argv [l_argc++] = l_tok;

		l_argv [l_argc] = NULL;

		/* Try each candidate name, first next to this binary, then
		 * on the PATH; the first that execs wins			*/
		for ( l_i = 0; l_i < l_nn; l_i++ )
			{
			l_argv[0] = (char *) l_names[l_i];

			if ( a_argv0 && (l_slash = strrchr(a_argv0, '/')) )
				{
				snprintf (l_sib, sizeof(l_sib), "%.*s/%s",
					  (int) (l_slash - a_argv0), a_argv0, l_names[l_i]);
				execv (l_sib, l_argv);
				}

			execvp (l_names[l_i], l_argv);
			}

		/* every exec failed - leave a distinct code for the parent	*/
		_exit (127);
		}

	if ( l_pid > 0 )
		{
		int	l_st = 0;

		waitpid (l_pid, &l_st, 0);

		/* The browser ran (even if it reported NOTFOUND) - done	*/
		if ( WIFEXITED(l_st) && (WEXITSTATUS(l_st) != 127) )
			return;
		}

fallback:
	/* The table driven help; map the first key to a verb		*/
	if ( a_keys && *a_keys )
		{
		for ( l_v = s_verbs; $ASCLEN(&l_v->name); l_v++ )
			if ( !strncasecmp(a_keys, (char *) $ASCPTR(&l_v->name), strcspn(a_keys, " ")) )
				{
				__cli$help(s_verbs, l_v);
				return;
				}
		}

	__cli$help(s_verbs, NULL);
}


/*
 *  DESCRIPTION: Scan the command line for a help request and serve it before
 *		the CLI parse. The recognized forms:
 *		  yanetchk                       - the overview
 *		  yanetchk /HELP | -h | --help   - the overview
 *		  yanetchk help [topic ...]      - the topic path
 *		  yanetchk <verb> /HELP [topic]  - the verb topic (subpath)
 *		All the non-switch tokens on a help carrying line are taken
 *		as the topic path keys, in order.
 *
 *  INPUTS:
 *	a_argc, a_argv:	the process arguments
 *
 *  OUTPUTS:
 *	NONE
 *
 *  RETURNS:
 *	non-zero when the request has been recognized and served
 */
static int	s_help_request	(int a_argc, char *a_argv[])
{
char	l_keys[256] = {0};
int	l_i, l_hit = 0, l_n = 0;

	if ( a_argc < 2 )
		{
		s_serve_help("", a_argv[0]);
		return	1;
		}

	/* The HELP pseudo-verb						*/
	if (    (0 == strncasecmp(a_argv[1], "HELP", 4)) && !a_argv[1][4] )
		l_hit = 1;

	for ( l_i = 1 + l_hit; l_i < a_argc; l_i++ )
		{
		if ( s_is_help_switch(a_argv[l_i]) )
			{
			l_hit = 1;
			continue;
			}

		/* A non-switch token is a topic path key		*/
		if ( (*a_argv[l_i] != '-') && (*a_argv[l_i] != '/') )
			l_n += snprintf(l_keys + l_n, sizeof(l_keys) - l_n, "%s%s", l_n ? " " : "", a_argv[l_i]);
		}

	if ( !l_hit )
		return	0;

	s_serve_help(l_keys, a_argv[0]);

	return	1;
}


/*
 *  DESCRIPTION: Validate and finalize configuration options: default ports,
 *		operational mode detection, interface name resolution,
 *		mode dependent DATALEN clamping, pacing interval computation.
 *
 *  INPUTS:
 *	NONE (implicit: g_* configuration variables)
 *
 *  OUTPUTS:
 *	g_lpn, g_tpn, g_mode, g_ifindex, g_datalen, g_delta_ts
 *
 *  RETURNS:
 *	STS$K_SUCCESS or error condition from $LOG
 */
static int	s_config_validation	(void)
{
int	l_sd;
struct ifreq	l_ifr = {0};

	/* The mode and the port defaults have been settled by the CLI verb
	 * dispatch (the action routines and the descriptor <defval> fields) */
	$LOG(STS$K_INFO, "Mode is: %s", (g_mode == YNETCHK$K_MODE_XMIT) ? "Sender" : "Receiver");

	/* Resolve the interface name BEFORE the DATALEN validation - the
	 * minimal size depends on it (a full frame vs a pure payload)	*/
	if ( $ASCLEN(&g_ifname) )
		{
		if ( !(g_ifindex = if_nametoindex((char *) $ASCPTR(&g_ifname))) )
			return	$LOG(STS$K_ERROR, "Cannot translate IFname: <%s> to IFindex, errno: %d",
				$ASCPTR(&g_ifname), errno);

		$LOG(STS$K_INFO, "IFname: <%s>, IFindex: %d", $ASCPTR(&g_ifname), g_ifindex);
		}

	/* /RING is an AF_PACKET-only accelerator - it needs a raw interface */
	if ( g_ring && !g_ifindex )
		{
		$LOG(STS$K_WARN, "/RING ignored: it requires the AF_PACKET engine (/IFNAME)");
		g_ring = 0;
		}

	if ( g_ifindex && (g_mode == YNETCHK$K_MODE_XMIT) )
		{
		/* AF_PACKET: the kernel does not fill the source IP in; when
		 * /LIP is omitted - take the primary IPv4 address of the
		 * /IFNAME interface itself					*/
		if ( !g_lip )
			{
			if ( 0 > (l_sd = socket(AF_INET, SOCK_DGRAM, 0)) )
				return	$LOG(STS$K_ERROR, "socket()->%d, errno: %d", l_sd, errno);

			{
			int	l_n = (int) $ASCLEN(&g_ifname);
			if ( l_n > (IFNAMSIZ - 1) )
				l_n = IFNAMSIZ - 1;
			memcpy (l_ifr.ifr_name, (char *) $ASCPTR(&g_ifname), l_n);
			l_ifr.ifr_name[l_n] = '\0';
			}

			if ( 0 > ioctl(l_sd, SIOCGIFADDR, &l_ifr) )
				return	close(l_sd),
					$LOG(STS$K_ERROR, "No IPv4 address on IF <%s>, ioctl(SIOCGIFADDR) errno: %d;"
					     " assign an address or give /LIP explicitly",
					     $ASCPTR(&g_ifname), errno);

			g_lip = ((struct sockaddr_in *) &l_ifr.ifr_addr)->sin_addr.s_addr;

			close(l_sd);

			$LOG(STS$K_INFO, "LIP is taken from IF <%s>: " $IPv4_BYTES_FMT,
			     $ASCPTR(&g_ifname), $IPv4_BYTES(g_lip));
			}

		/* The destination MAC (next hop): the /TMAC option overrides
		 * the compile time default g_tmac; the string form has been
		 * validated by the library CLI$K_MAC type - both the colon
		 * and the dash separated forms are accepted		*/
		if ( $ASCLEN(&g_tmac_str) )
			{
			if (    (ETH_ALEN != sscanf((char *) $ASCPTR(&g_tmac_str),
						"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
						&g_tmac[0], &g_tmac[1], &g_tmac[2],
						&g_tmac[3], &g_tmac[4], &g_tmac[5]))
			     && (ETH_ALEN != sscanf((char *) $ASCPTR(&g_tmac_str),
						"%hhx-%hhx-%hhx-%hhx-%hhx-%hhx",
						&g_tmac[0], &g_tmac[1], &g_tmac[2],
						&g_tmac[3], &g_tmac[4], &g_tmac[5])) )
				return	$LOG(STS$K_ERROR, "Cannot parse /TMAC=<%s>, expected aa:bb:cc:dd:ee:ff",
					$ASCPTR(&g_tmac_str));
			}

		$LOG(STS$K_INFO, "Target MAC (next hop): %02x:%02x:%02x:%02x:%02x:%02x%s",
		     g_tmac[0], g_tmac[1], g_tmac[2], g_tmac[3], g_tmac[4], g_tmac[5],
		     $ASCLEN(&g_tmac_str) ? "" : " (default)");

		/* DATALEN is the full frame length: Ethernet + IP + UDP + payload_t at least */
		g_datalen = $RANGE(g_datalen, (int) (YNETCHK$K_HDRSZ + sizeof(struct payload_t)), YNETCHK$K_BUF_SIZE);

		$LOG(STS$K_INFO, "Full frame size after validation: %d octets", g_datalen);
		}
	else	{
		g_datalen = $RANGE(g_datalen, (int) sizeof(struct payload_t), YNETCHK$K_BUF_SIZE);

		$LOG(STS$K_INFO, "Payload size after validation: %d octets", g_datalen);
		}

	g_delta_ts.tv_sec  = g_delta_ns / 1000000000;
	g_delta_ts.tv_nsec = (long) (g_delta_ns % 1000000000);

	$LOG(STS$K_INFO, "Delta: %d nsec (%ld sec, %ld nanosec)",
	     g_delta_ns, (long) g_delta_ts.tv_sec, (long) g_delta_ts.tv_nsec);

	/*
	 * The calculated offered load of the generator. The on-wire L2 frame
	 * is g_datalen in AF_PACKET mode (already the full Ethernet frame) or
	 * g_datalen + YNETCHK$K_HDRSZ in UDP mode (payload plus Eth/IP/UDP).
	 * "wire" adds the 24 octets the link spends per frame but the frame
	 * does not carry - FCS (4) + preamble/SFD (8) + inter-frame gap (12) -
	 * so it reflects the real line utilisation, not just the frame bytes.
	 */
	if ( g_mode == YNETCHK$K_MODE_XMIT )
		{
		int	l_l2   = g_ifindex ? g_datalen : (g_datalen + (int) YNETCHK$K_HDRSZ);
		int	l_wire = l_l2 + 24;

		if ( g_delta_ns > 0 )
			{
			double	l_pps    = 1.0e9 / (double) g_delta_ns;
			double	l_bw_l2  = l_pps * (double) l_l2   * 8.0 / 1.0e6;
			double	l_bw_wir = l_pps * (double) l_wire * 8.0 / 1.0e6;

			$LOG(STS$K_INFO, "Calculated load: %.0f pps, L2 %.3f Mbit/s (%d octets/frame), "
			     "wire %.3f Mbit/s (%d octets/frame incl. FCS+preamble+IFG)",
			     l_pps, l_bw_l2, l_l2, l_bw_wir, l_wire);
			}
		else	$LOG(STS$K_INFO, "Calculated load: line rate (/DELTA=0, unbounded), "
			     "L2 %d octets/frame, wire %d octets/frame", l_l2, l_wire);
		}

	return	STS$K_SUCCESS;
}




/*
 *  DESCRIPTION: Thin void*-returning trampolines for pthread_create(). The
 *		worker routines return an int status (via $LOG on the error
 *		paths), which does not match the pthread start-routine type;
 *		these wrappers give pthread the exact void *(*)(void *) type
 *		without the incompatible function-pointer cast.
 *
 *  INPUTS:
 *	a_arg:	passed through to the worker (unused by the workers)
 *
 *  OUTPUTS:
 *	NONE
 *
 *  RETURNS:
 *	NULL (the worker's int status is consumed here)
 */
static void *	s_thr_receiver	(void *a_arg)	{ (void) s_thread_receiver (a_arg);  return NULL; }
static void *	s_thr_receiver2	(void *a_arg)	{ (void) s_thread_receiver2 (a_arg); return NULL; }
static void *	s_thr_receiver3	(void *a_arg)	{ (void) s_thread_receiver3 (a_arg); return NULL; }
static void *	s_thr_sender	(void *a_arg)	{ (void) s_thread_sender (a_arg);    return NULL; }
static void *	s_thr_sender2	(void *a_arg)	{ (void) s_thread_sender2 (a_arg);   return NULL; }
static void *	s_thr_sender3	(void *a_arg)	{ (void) s_thread_sender3 (a_arg);   return NULL; }


int	main	(int argc, char *argv[])
{
int	l_rc;
pthread_t	l_tid;
CLI_CTX	*l_clictx = NULL;
uint64_t	l_total = 0, l_lost = 0, l_reord = 0, l_dup = 0,
		l_total_now, l_lost_now, l_seq_now, l_delta_ns_now,
		l_reord_now, l_dup_now, l_reordmax_now;
uint64_t	l_pps, l_bw_kbps, l_elapsed_ns, l_dpkts, l_octets = 0, l_epoch = 0;
int		l_wire_bytes;
struct timespec	l_t_prev, l_t_now;

	$LOG(STS$K_INFO, "Rev: " __IDENT__ "/"  __ARCH__NAME__   ", (built  at "__DATE__ " " __TIME__ " with CC " __VERSION__ ")");

	/* Register the YANETCHK message catalogue with the UTILITY$ROUTINES
	 * package so that $PUTMSG() can format the receiver diagnostics	*/
	__util$inimsg (&s_msgdesc);

	/*
	 * A help request is served through the HLP_ROUTINES library BEFORE the
	 * CLI parse (so 'send /HELP' works despite the required /TIP); see
	 * s_help_request() for the recognized forms and the topic path rules.
	 */
	if ( s_help_request(argc, argv) )
		return	EXIT_SUCCESS;

	if ( !(1 & (l_rc = __cli$parse(s_verbs, CLI$M_OPSIGNAL, argc - 1, argv + 1, (void **) &l_clictx))) )
		return	(l_rc == STS$K_WARN)
			? EXIT_SUCCESS					/* /HELP has been served	*/
			: $LOG(STS$K_ERROR, "Error processing the command line");

	if ( !(1 & (l_rc = __cli$dispatch(l_clictx))) )
		return	$LOG(STS$K_ERROR, "Error dispatching the command, status: %#x", l_rc);

	__cli$cleanup(l_clictx);

	if ( $ASCLEN(&g_logfspec) )
		{
		__util$deflog((char *) $ASCPTR(&g_logfspec), NULL);

		$LOG(STS$K_INFO, "Rev: " __IDENT__ "/"  __ARCH__NAME__   ", (built  at "__DATE__ " " __TIME__ " with CC " __VERSION__ ")");
		}

	if ( !(1 & s_config_validation()) )
		return	$LOG(STS$K_FATAL, "Abort execution, check configuration!!!");

	s_init_sig_handler ();

	if ( g_mode == YNETCHK$K_MODE_RCVR )
		{
		if ( (l_rc = pthread_create(&l_tid, NULL,
			g_ifindex ? (g_ring ? s_thr_receiver3 : s_thr_receiver2) : s_thr_receiver, NULL)) )
			return	$LOG(STS$K_ERROR, "Cannot start <s_thread_receiver%s> thread, pthread_create()->%d, errno=%d",
				g_ifindex ? (g_ring ? "3" : "2") : "", l_rc, errno);
		}
	else	if ( (l_rc = pthread_create(&l_tid, NULL,
			g_ifindex ? (g_ring ? s_thr_sender3 : s_thr_sender2) : s_thr_sender, NULL)) )
			return	$LOG(STS$K_ERROR, "Cannot start <s_thread_sender%s> thread, pthread_create()->%d, errno=%d",
				g_ifindex ? "2" : "", l_rc, errno);

	/* The on-wire frame size for the bandwidth figures: the AF_PACKET
	 * frame already carries the Ethernet header, the UDP payload needs it
	 * added; +24 accounts for FCS (4) + preamble/SFD (8) + IFG (12).	*/
	l_wire_bytes = (g_ifindex ? g_datalen : (g_datalen + (int) YNETCHK$K_HDRSZ)) + 24;

	clock_gettime(CLOCK_MONOTONIC, &l_t_prev);

	while ( !g_exit_flag )
		{
		for (int l_delay = 3;  (l_delay = sleep(l_delay)); );

		/* The real elapsed interval - sleep() can be cut short by a
		 * signal, so measure it rather than assume the nominal 3 s	*/
		clock_gettime(CLOCK_MONOTONIC, &l_t_now);
		l_elapsed_ns = ((uint64_t) (l_t_now.tv_sec  - l_t_prev.tv_sec)) * 1000000000ULL
			     + ((uint64_t) l_t_now.tv_nsec - (uint64_t) l_t_prev.tv_nsec);
		l_t_prev = l_t_now;
		if ( l_elapsed_ns == 0 )
			l_elapsed_ns = 1;				/* Guard against a divide-by-zero	*/

		/* Read the atomics once per iteration - a consistent snapshot
		 * for the both increments and absolute values			*/
		l_total_now    = s_result.total;
		l_seq_now      = s_result.seq;
		l_lost_now     = s_result.lost;
		l_delta_ns_now = s_result.delta_ns;
		l_reord_now    = s_result.reord;
		l_dup_now      = s_result.dup;
		l_reordmax_now = s_result.reord_max;

		/* A resync since the last look? The receiver zeroed the session
		 * counters and bumped the epoch; rebase our own baselines to 0
		 * so the increments and the rate are measured from the new
		 * session, not driven negative by the counter reset.		*/
		if ( s_result.epoch != l_epoch )
			{
			l_epoch  = s_result.epoch;
			l_total  = 0;
			l_lost   = 0;
			l_reord  = 0;
			l_dup    = 0;
			l_octets = 0;
			}

		/* Measured rate over the elapsed interval. pps = pkts * 1e9/ns.
		 * Bandwidth is wire kbit/s = octets * 8 bits, scaled to the
		 * second (1e9/ns) and to kbit (/1000); the receiver uses the
		 * octets actually taken off the wire (its own /DATALEN may not
		 * match the sender's), the transmitter uses its fixed frame.
		 * +24 octets/pkt (FCS + preamble/SFD + IFG) is the link cost
		 * per frame. All integer: pkts~1e6 * bytes~1.5e3 * 8 * 1e6 < u64.*/
		l_dpkts   = l_total_now - l_total;
		l_pps     = (l_dpkts * 1000000000ULL) / l_elapsed_ns;

		if ( g_mode == YNETCHK$K_MODE_RCVR )
			{
			uint64_t	l_octets_now = s_result.octets;
			uint64_t	l_wire_octets = (l_octets_now - l_octets) + l_dpkts * 24ULL;

			l_bw_kbps = (l_wire_octets * 8ULL * 1000000ULL) / l_elapsed_ns;
			l_octets  = l_octets_now;
			}
		else	l_bw_kbps = (l_dpkts * (uint64_t) l_wire_bytes * 8ULL * 1000000ULL) / l_elapsed_ns;

		if ( g_mode == YNETCHK$K_MODE_RCVR )
			{
			if ( (l_total_now - l_total) == 0 )
				$PUTMSG_FAO(YNETCHK$_NOTRAFFIC, l_total_now,
				     l_lost_now,
				     l_reord_now,
				     l_reordmax_now,
				     l_dup_now,
				     l_seq_now);
			else	$PUTMSG_FAO(YNETCHK$_STATS, (l_total_now - l_total),
				     l_total_now,
				     (l_lost_now - l_lost),
				     l_lost_now,
				     (l_reord_now - l_reord),
				     l_reord_now,
				     l_reordmax_now,
				     (l_dup_now - l_dup),
				     l_dup_now,
				     l_seq_now,
				     l_delta_ns_now,
				     l_pps,
				     l_bw_kbps);
			}
		else	$PUTMSG_FAO(YNETCHK$_SENT, (l_total_now - l_total),
			     l_total_now,
			     l_seq_now,
			     l_pps,
			     l_bw_kbps);

		l_total = l_total_now;
		l_lost  = l_lost_now;
		l_reord = l_reord_now;
		l_dup   = l_dup_now;
		}

	/*
	 * On shutdown, in the receiver mode, the sequences still pending in
	 * the window (expected, never arrived) are the residual loss - fold
	 * them into the final count and print the closing tally.
	 */
	if ( g_mode == YNETCHK$K_MODE_RCVR )
		{
		uint64_t	l_e, l_lo;

		l_lo = (s_result.win_hi >= (YNETCHK$K_WINSZ - 1))
			? (s_result.win_hi - (YNETCHK$K_WINSZ - 1)) : 1;

		if ( l_lo < s_result.win_base )				/* Do not blame pre-start phantom seqs	*/
			l_lo = s_result.win_base;

		for ( l_e = l_lo; (l_e >= 1) && (l_e <= s_result.win_hi); l_e++ )
			if ( !s_win_test(l_e) )
				s_result.lost += 1;

		$PUTMSG_FAO(YNETCHK$_FINAL, s_result.total,
		     s_result.lost,
		     s_result.reord,
		     s_result.reord_max,
		     s_result.dup,
		     s_result.win_hi);
		}

	return	EXIT_SUCCESS;
}
