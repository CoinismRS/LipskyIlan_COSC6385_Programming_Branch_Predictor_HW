// test_loop.cc
// Unit tests for the loop predictor buffer (LPB) inside pm_predictor.
// Each test drives synthetic conditional-branch streams through the
// predictor and checks the LPB's behaviour directly.
//
// Build & run:  cd src && make test

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "branch.h"
#include "predictor.h"
#include "my_predictor.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; \
	printf ("  FAIL line %d: ", __LINE__); printf (__VA_ARGS__); printf ("\n"); } } while (0)

struct result { long miss, loop_used, loop_miss; };

// Feed one branch outcome; return the pm_update seen at prediction time.
static pm_update step (pm_predictor &p, unsigned int addr, bool taken, bool &pred) {
	branch_info b; b.address = addr; b.opcode = 0; b.br_flags = BR_CONDITIONAL;
	pm_update *u = (pm_update *) p.predict (b);
	pm_update copy = *u;
	pred = u->direction_prediction ();
	p.update (u, taken, 0);
	return copy;
}

// Run `runs` iterations of a loop: `body` outcomes of direction `dir`
// followed by one exit (!dir).  Counts are taken only over the last
// `measure` runs.
static result run_loop (pm_predictor &p, unsigned int addr, int body, bool dir,
                        int runs, int measure) {
	result r = { 0, 0, 0 };
	for (int k = 0; k < runs; k++) {
		for (int i = 0; i <= body; i++) {
			bool taken = (i < body) ? dir : !dir, pred;
			pm_update u = step (p, addr, taken, pred);
			if (k >= runs - measure) {
				r.miss += (pred != taken);
				if (u.loop_used) { r.loop_used++; r.loop_miss += (u.loop_pred != taken); }
			}
		}
	}
	return r;
}

// Default (submitted) configuration.  Tests 1-5 check the LPB's own
// prediction (u.loop_pred / u.loop_used), independent of the global table.
static pm_config loop_only (void) {
	pm_config c;
	return c;
}

int main (void) {
	// ---- 1. trip counts the LPB must learn exactly ----------------------
	int bodies[] = { 1, 3, 10, 20, 40, 62, 63 };
	for (unsigned k = 0; k < sizeof bodies / sizeof bodies[0]; k++) {
		pm_predictor p (loop_only ());
		int n = bodies[k];
		run_loop (p, 0x401000, n, true, 5, 0);                 // train
		result r = run_loop (p, 0x401000, n, true, 20, 20);     // measure
		CHECK (r.loop_used == 20L * (n + 1),
		       "taken loop, body %d: LPB used on %ld of %d branches", n, r.loop_used, 20 * (n + 1));
		CHECK (r.loop_miss == 0, "taken loop, body %d: %ld LPB mispredictions", n, r.loop_miss);
		printf ("taken loop    body=%2d : LPB used %4ld/%4d, LPB misses %ld, total misses %ld\n",
		        n, r.loop_used, 20 * (n + 1), r.loop_miss, r.miss);
	}

	// ---- 2. "not-taken" loops (exit on taken) ---------------------------
	{
		pm_predictor p (loop_only ());
		run_loop (p, 0x402040, 12, false, 6, 0);
		result r = run_loop (p, 0x402040, 12, false, 20, 20);
		CHECK (r.loop_used == 20 * 13 && r.loop_miss == 0,
		       "not-taken loop: used %ld, misses %ld", r.loop_used, r.loop_miss);
		printf ("not-taken loop body=12 : LPB used %4ld/%4d, LPB misses %ld\n", r.loop_used, 20 * 13, r.loop_miss);
	}

	// ---- 3. loops longer than the 6-bit Count are never used ------------
	{
		pm_predictor p (loop_only ());
		run_loop (p, 0x403000, 64, true, 5, 0);
		result r = run_loop (p, 0x403000, 64, true, 10, 10);
		CHECK (r.loop_used == 0, "body 64 (too long): LPB used %ld times", r.loop_used);
		r = run_loop (p, 0x403000, 100, true, 10, 10);
		CHECK (r.loop_used == 0, "body 100 (too long): LPB used %ld times", r.loop_used);
		printf ("too-long loops (64, 100): LPB never used\n");
	}

	// ---- 4. a change of trip count stops LPB use until it repeats -------
	{
		pm_predictor p (loop_only ());
		run_loop (p, 0x404000, 8, true, 6, 0);
		// one run of a different length: the exit comes "late"
		result r = run_loop (p, 0x404000, 11, true, 1, 1);
		CHECK (r.loop_miss == 1, "changed trip count: expected 1 LPB miss, got %ld", r.loop_miss);
		// the next loop_conf runs re-confirm the new length: LPB not used
		r = run_loop (p, 0x404000, 11, true, p.cfg.loop_conf, p.cfg.loop_conf);
		CHECK (r.loop_used == 0, "after change: LPB used %ld times before re-confirming", r.loop_used);
		// then it is confident again and perfect
		r = run_loop (p, 0x404000, 11, true, 5, 5);
		CHECK (r.loop_used == 5 * 12 && r.loop_miss == 0,
		       "after re-learning: used %ld, misses %ld", r.loop_used, r.loop_miss);
		printf ("trip count 8 -> 11       : 1 miss, re-learned after %d confirming runs\n", p.cfg.loop_conf);
	}

	// ---- 5. LPB fields: index IP[9:4], tag IP[15:10], 2 ways, LRU -------
	{
		pm_predictor p (loop_only ());
		bool pred;
		unsigned int a = 0x00000120;   // IP[9:4] = 0x12, IP[15:10] = 0
		unsigned int b = a | (1 << 10); // same set, tag 1
		unsigned int c = a | (2 << 10); // same set, tag 2
		unsigned int d = a + 0x8;       // same 16-byte block as a -> aliases with a
		pm_update u = step (p, a, true, pred);
		CHECK (u.lset == 0x12 && u.ltag == 0, "a: set %u tag %u", u.lset, u.ltag);
		u = step (p, b, true, pred);
		CHECK (u.lset == 0x12 && u.ltag == 1, "b: set %u tag %u", u.lset, u.ltag);
		u = step (p, d, true, pred);
		CHECK (u.lset == 0x12 && u.ltag == 0, "d aliases with a: set %u tag %u", u.lset, u.ltag);
		// allocate a, b, c into the same 2-way set; c must evict LRU (a)
		pm_predictor q (loop_only ());
		step (q, a, false, pred);  // bimodal says T (init 2) -> miss -> allocate a
		step (q, b, false, pred);  // allocate b
		step (q, c, false, pred);  // allocate c, evicting a (LRU)
		int s = 0x12 * q.cfg.loop_ways;
		bool has_a = false, has_b = false, has_c = false;
		for (int w = 0; w < q.cfg.loop_ways; w++) {
			if (!q.l_valid[s + w]) continue;
			has_a |= q.l_tag[s + w] == 0; has_b |= q.l_tag[s + w] == 1; has_c |= q.l_tag[s + w] == 2;
		}
		CHECK (!has_a && has_b && has_c, "LRU replacement: a=%d b=%d c=%d", has_a, has_b, has_c);
		CHECK (q.n_loop_alloc == 3, "allocations: %lld", q.n_loop_alloc);
		printf ("LPB index/tag fields, aliasing and LRU replacement OK\n");
	}

	// ---- 6. end to end: loop predictor removes the exit mispredictions --
	{
		// Loop with body 30 (longer than the 15-bit global history) plus an
		// unrelated random branch inside the body.
		pm_config off; off.use_loop = 0;
		pm_predictor with_lpb, without (off);
		long miss_with = 0, miss_without = 0;
		unsigned int lfsr = 0xACE1u;
		for (int run = 0; run < 2000; run++)
			for (int i = 0; i <= 30; i++) {
				lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
				bool r = lfsr & 1, pred;
				step (with_lpb, 0x405010, r, pred);
				step (without, 0x405010, r, pred);
				bool t = i < 30;
				step (with_lpb, 0x405100, t, pred); if (run >= 1000) miss_with += pred != t;
				step (without, 0x405100, t, pred);  if (run >= 1000) miss_without += pred != t;
			}
		CHECK (miss_with == 0, "end-to-end: %ld loop-branch misses with LPB", miss_with);
		CHECK (miss_without >= 900, "end-to-end: only %ld misses without LPB", miss_without);
		printf ("end-to-end, 1000 runs of a 30-iteration loop: loop-branch misses %ld without LPB, %ld with LPB\n",
		        miss_without, miss_with);
	}

	if (failures == 0) printf ("\nPASS: all %d loop predictor checks.\n", checks);
	else printf ("\nFAIL: %d of %d checks\n", failures, checks);
	return failures ? 1 : 0;
}
