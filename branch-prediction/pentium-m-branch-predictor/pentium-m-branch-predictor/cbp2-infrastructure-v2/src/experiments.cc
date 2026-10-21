// experiments.cc
// Design-space driver used for REPORT.md.  Reads one trace once and feeds
// every branch to a whole set of predictors in parallel, then prints one CSV
// row per predictor:
//
//   trace,predictor,cond_branches,mispredictions,mpki,global_hit_rate,
//   global_hit_accuracy,bimodal_used_accuracy,bimodal_alone_accuracy,allocations,
//   loop_used_rate,loop_used_accuracy,loop_allocations
//
// Usage:  ./experiments <trace-file>          (see ../run_experiments.sh)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "branch.h"
#include "trace.h"
#include "predictor.h"
#include "my_predictor.h"

// Plain gshare with a configurable size, used as a baseline at a storage
// budget comparable to the Pentium M tables.
class gshare_n : public branch_predictor {
public:
	branch_update u;
	branch_info bi;
	unsigned int bits, history, index;
	std::vector<unsigned char> tab;
	gshare_n (unsigned int b) : bits(b), history(0), tab(1u << b, 2) {}
	branch_update *predict (branch_info &b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			index = (history ^ b.address) & ((1u << bits) - 1);
			u.direction_prediction (tab[index] >> 1);
		} else u.direction_prediction (true);
		u.target_prediction (0);
		return &u;
	}
	void update (branch_update *, bool taken, unsigned int) {
		if (!(bi.br_flags & BR_CONDITIONAL)) return;
		unsigned char &c = tab[index];
		if (taken) { if (c < 3) c++; } else { if (c > 0) c--; }
		history = ((history << 1) | taken) & ((1u << bits) - 1);
	}
};

// Bimodal-only predictor: pm_predictor whose global table never allocates.
class bimodal_only : public pm_predictor {
public:
	bimodal_only (void) {}
	branch_update *predict (branch_info &b) {
		branch_update *r = pm_predictor::predict (b);
		if (b.br_flags & BR_CONDITIONAL) r->direction_prediction (u.bimodal_pred);
		return r;
	}
	void update (branch_update *bu, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) train (bim[((pm_update *) bu)->index], taken);
	}
};

struct entry {
	std::string name;
	branch_predictor *p;
	pm_predictor *pm;    // non-null if it is a pm_predictor (for stats)
	long long cond, miss;
};

static void add_pm (std::vector<entry> &v, const char *name, pm_config c) {
	pm_predictor *p = new pm_predictor (c);
	entry e = { name, p, p, 0, 0 };
	v.push_back (e);
}

int main (int argc, char *argv[]) {
	if (argc != 2) { fprintf (stderr, "Usage: %s <trace>\n", argv[0]); exit (1); }

	std::vector<entry> v;
	pm_config lp;                       // default = submitted predictor (with LPB)
	pm_config base = lp; base.use_loop = 0;   // bimodal + global only

	// ---- baselines -------------------------------------------------------
	{ entry e = { "always_taken", 0, 0, 0, 0 }; v.push_back (e); }
	{ bimodal_only *b = new bimodal_only (); entry e = { "bimodal_4K", b, 0, 0, 0 }; v.push_back (e); }
	{ entry e = { "gshare_4K_h12", new gshare_n (12), 0, 0, 0 }; v.push_back (e); }
	{ entry e = { "gshare_16K_h14", new gshare_n (14), 0, 0, 0 }; v.push_back (e); }
	{ entry e = { "gshare_32K_h15", new gshare_predictor (), 0, 0, 0 }; v.push_back (e); }

	// ---- the submitted configuration (bimodal + global + loop) ----------
	add_pm (v, "pm_loop", lp);

	// ======== Part 1: bimodal + global predictor only (no LPB) ===========
	add_pm (v, "pm_noloop", base);

	// ---- policy variants -------------------------------------------------
	{ pm_config c = base; c.alloc_policy = PM_ALLOC_ALWAYS; c.alloc_counter = PM_CTR_KEEP;
	  add_pm (v, "pm_example_policy", c); }      // exactly the handout's policies
	{ pm_config c = base; c.alloc_policy = PM_ALLOC_ALWAYS;
	  add_pm (v, "pm_alloc_always", c); }
	{ pm_config c = base; c.alloc_counter = PM_CTR_KEEP;
	  add_pm (v, "pm_ctr_keep", c); }

	// ---- associativity / size -------------------------------------------
	{ pm_config c = base; c.ways = 2; add_pm (v, "pm_2way_512set", c); }
	{ pm_config c = base; c.ways = 2; c.set_bits = 10; add_pm (v, "pm_2way_1024set", c); }
	{ pm_config c = base; c.ways = 1; add_pm (v, "pm_1way_512set", c); }
	{ pm_config c = base; c.ways = 8; add_pm (v, "pm_8way_512set", c); }

	// ---- hash function ---------------------------------------------------
	{ pm_config c = base; c.hash_mode = PM_HASH_PM_GHR; add_pm (v, "pm_hash_pmbits_ghr", c); }
	{ pm_config c = base; c.hash_mode = PM_HASH_PM_PIR; add_pm (v, "pm_hash_pmbits_pir", c); }

	// ---- history length (tag stays 6 bits, sets = hist - 6 bits) --------
	{ pm_config c = base; c.hist_bits = 12; c.set_bits = 6;  c.ways = 32; add_pm (v, "pm_h12_64x32", c); }
	{ pm_config c = base; c.hist_bits = 13; c.set_bits = 7;  c.ways = 16; add_pm (v, "pm_h13_128x16", c); }
	{ pm_config c = base; c.hist_bits = 14; c.set_bits = 8;  c.ways = 8;  add_pm (v, "pm_h14_256x8", c); }
	{ pm_config c = base; c.hist_bits = 16; c.set_bits = 10; c.ways = 2;  add_pm (v, "pm_h16_1024x2", c); }
	{ pm_config c = base; c.hist_bits = 17; c.set_bits = 11; c.ways = 1;  add_pm (v, "pm_h17_2048x1", c); }

	// ======== Part 2: loop predictor variants =============================
	// priority between the global table and the LPB  x  confidence
	// threshold (0 = use as soon as a Limit is learned)
	for (int g = 0; g <= 1; g++)
		for (int t = 0; t <= 3; t++) {
			static char names[8][32];
			pm_config c = lp; c.loop_over_global = g; c.loop_conf = t;
			snprintf (names[g * 4 + t], 32, "lp_%s_conf%d", g ? "loopfirst" : "globalfirst", t);
			add_pm (v, names[g * 4 + t], c);
		}
	// global allocation trigger with the LPB present
	{ pm_config c = lp; c.alloc_policy = PM_ALLOC_BIMODAL_WRONG; add_pm (v, "lp_galloc_bimodal_wrong", c); }
	// LPB size / organisation
	{ pm_config c = lp; c.loop_set_bits = 5; add_pm (v, "lp_64entries", c); }
	{ pm_config c = lp; c.loop_set_bits = 7; add_pm (v, "lp_256entries", c); }
	{ pm_config c = lp; c.loop_set_bits = 8; add_pm (v, "lp_512entries", c); }
	{ pm_config c = lp; c.loop_ways = 4; c.loop_set_bits = 5; add_pm (v, "lp_128_4way", c); }
	{ pm_config c = lp; c.loop_shift = 0; add_pm (v, "lp_index_ip5_0", c); }
	{ pm_config c = lp; c.loop_count_bits = 8; add_pm (v, "lp_count8bit", c); }

	init_trace (argv[1]);
	for (;;) {
		trace *t = read_trace ();
		if (!t) break;
		bool cond = t->bi.br_flags & BR_CONDITIONAL;
		for (size_t i = 0; i < v.size (); i++) {
			entry &e = v[i];
			bool pred;
			branch_update *u = 0;
			if (e.p) { u = e.p->predict (t->bi); pred = u->direction_prediction (); }
			else pred = true;
			if (cond) { e.cond++; e.miss += (pred != t->taken); }
			if (e.p) e.p->update (u, t->taken, t->target);
		}
	}
	end_trace ();

	// trace name = directory component (e.g. 164.gzip)
	std::string tn = argv[1];
	size_t s = tn.rfind ('/');
	if (s != std::string::npos) { size_t s2 = tn.rfind ('/', s - 1); tn = tn.substr (s2 == std::string::npos ? 0 : s2 + 1, s - (s2 == std::string::npos ? 0 : s2 + 1)); }

	for (size_t i = 0; i < v.size (); i++) {
		entry &e = v[i];
		printf ("%s,%s,%lld,%lld,%.3f", tn.c_str (), e.name.c_str (), e.cond, e.miss, 1000.0 * (e.miss / 1e8));
		if (e.pm && e.pm->n_cond) {
			pm_predictor *p = e.pm;
			printf (",%.4f,%.4f,%.4f,%.4f,%lld,%.4f,%.4f,%lld",
			        (double) p->n_ghit / p->n_cond,
			        p->n_ghit ? (double) p->n_ghit_correct / p->n_ghit : 0.0,
			        p->n_bim_used ? (double) p->n_bim_used_correct / p->n_bim_used : 0.0,
			        (double) p->n_bim_correct / p->n_cond,
			        p->n_alloc,
			        (double) p->n_loop_used / p->n_cond,
			        p->n_loop_used ? (double) p->n_loop_correct / p->n_loop_used : 0.0,
			        p->n_loop_alloc);
		} else printf (",,,,,,,,");
		printf ("\n");
	}
	return 0;
}
