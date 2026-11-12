/*
 *  Deterministic unit test of the reordering-window loss/reorder/dup
 *  accounting, independent of any socket. Mirrors the window logic of
 *  s_rcv_account()/s_win_*() in src/yanetchk.c. The key regression it
 *  guards is the mid-stream start: a receiver whose first sequence is
 *  large must not charge the pre-start sequences as losses.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define WINLOG   12
#define WINSZ    (1u << WINLOG)
#define WINMASK  (WINSZ - 1)
#define WINWORDS (WINSZ / 64)
#define RESYNC   100000

static struct { uint64_t total, lost, reord, dup, reord_max, seq, win_hi, win_base, win_seen[WINWORDS]; } s;

static inline int  wt(uint64_t x){ return (s.win_seen[(x & WINMASK) >> 6] >> (x & 63)) & 1u; }
static inline void ws(uint64_t x){ s.win_seen[(x & WINMASK) >> 6] |=  (1ULL << (x & 63)); }
static inline void wc(uint64_t x){ s.win_seen[(x & WINMASK) >> 6] &= ~(1ULL << (x & 63)); }

static void wreset(uint64_t a){ memset(s.win_seen, 0, sizeof(s.win_seen)); s.win_hi = a; s.win_base = a; ws(a); s.seq = a; }

static void wadv(uint64_t a)
{
	uint64_t e, ev, lo;
	if ( (a - s.win_hi) >= WINSZ )
		{
		lo = (s.win_hi >= (WINSZ - 1)) ? (s.win_hi - (WINSZ - 1)) : 0;
		if ( lo < s.win_base ) lo = s.win_base;
		for ( e = lo; e <= s.win_hi; e++ ) if ( !wt(e) ) s.lost++;
		wreset(a); return;
		}
	for ( e = s.win_hi + 1; e <= a; e++ )
		{
		ev = e - WINSZ;
		if ( (e >= WINSZ) && (ev >= s.win_base) && !wt(ev) ) s.lost++;
		wc(e);
		}
	ws(a); s.win_hi = a; s.seq = a;
}

static void acct(uint64_t seq)
{
	uint64_t hi = s.win_hi, disp;
	s.total++;
	if ( !hi && !s.seq ) { wreset(seq); return; }
	if ( seq > hi ) { wadv(seq); return; }
	if ( (hi - seq) > RESYNC ) { wreset(seq); return; }
	disp = hi - seq; if ( disp > s.reord_max ) s.reord_max = disp;
	if ( disp >= WINSZ ) { s.reord++; return; }
	if ( wt(seq) ) { s.dup++; return; }
	ws(seq); s.reord++;
}

static int fails = 0;
static void chk(const char *n, uint64_t l, uint64_t r, uint64_t d)
{
	if ( s.lost != l || s.reord != r || s.dup != d )
		{ printf("FAIL %-26s lost=%llu(exp %llu) reord=%llu(exp %llu) dup=%llu(exp %llu)\n", n,
			(unsigned long long)s.lost,(unsigned long long)l,(unsigned long long)s.reord,(unsigned long long)r,
			(unsigned long long)s.dup,(unsigned long long)d); fails++; }
	else printf("ok   %-26s lost=%llu reord=%llu dup=%llu\n", n,(unsigned long long)l,(unsigned long long)r,(unsigned long long)d);
}

int main(void)
{
	const uint64_t S0 = 26000000ULL;

	memset(&s,0,sizeof(s)); for (uint64_t k=0;k<20000;k++) acct(S0+k);                 chk("midstream no-loss",0,0,0);
	memset(&s,0,sizeof(s)); for (uint64_t k=1;k<=20000;k++) acct(k);                    chk("from-1 in-order",0,0,0);
	memset(&s,0,sizeof(s)); for (uint64_t k=0;k<10000;k++){ if(S0+k==S0+100)continue; acct(S0+k);} chk("midstream real-loss",1,0,0);
	memset(&s,0,sizeof(s)); acct(S0);acct(S0+1);acct(S0+2);acct(S0+4);acct(S0+3); for(uint64_t k=5;k<9000;k++)acct(S0+k); chk("midstream reorder",0,1,0);
	memset(&s,0,sizeof(s)); for(uint64_t k=0;k<9000;k++){acct(S0+k); if(k==50)acct(S0+50);} chk("midstream dup",0,0,1);

	memset(&s,0,sizeof(s)); for(uint64_t k=0;k<9000;k++)acct(S0+k); { uint64_t lb=s.lost; for(uint64_t k=1;k<9000;k++)acct(k);
		if(s.lost!=lb){printf("FAIL restart added loss\n");fails++;}else printf("ok   restart clean            lost=%llu\n",(unsigned long long)s.lost);}

	printf(fails ? "\n=== %d FAILED ===\n" : "\n=== ALL PASS ===\n", fails);
	return fails ? 1 : 0;
}
