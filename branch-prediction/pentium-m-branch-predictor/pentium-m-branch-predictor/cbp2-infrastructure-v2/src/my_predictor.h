// my_predictor.h
// This file contains a sample gshare_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.

class gshare_update : public branch_update {
public:
	unsigned int index;
};

class gshare_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	15
#define TABLE_BITS	15
	gshare_update u;
	branch_info bi;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS];

	gshare_predictor (void) : history(0) { 
		memset (tab, 0, sizeof (tab));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = 
				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1));
			u.direction_prediction (tab[u.index] >> 1);
		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			unsigned char *c = &tab[((gshare_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1;
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
		}
	}
};

//
// Pentium M hybrid branch predictors
// This class implements a simple hybrid branch predictor based on the Pentium M branch outcome prediction units.
// Instead of implementing the complete Pentium M branch outcome predictors, the class below implements a hybrid
// predictor that combines a bimodal predictor and a global predictor.
//
// ---------------------------------------------------------------------------
// Implementation notes
// ---------------------------------------------------------------------------
// Structure (default configuration, sizes taken from Uzelac & Milenkovic,
// "Experiment Flows and Microbenchmarks for Reverse Engineering of Branch
// Predictor Structures", ISPASS 2009, and the assignment figure):
//
//   * Bimodal table : 4096 x 2-bit saturating counters, index = IP[11:0].
//   * Global table  : 512 sets x 4 ways (2048 entries).  Each entry holds
//                     {valid, 6-bit tag, 2-bit counter} plus LRU state.
//                     HASH[14:0] = IP[14:0] XOR GHR[14:0]
//                     set index  = HASH[14:6]  (9 bits)
//                     tag        = HASH[5:0]   (6 bits)
//                     The assignment allows us to replace the Path
//                     Information Register (PIR) with a plain global
//                     outcome-history register (GHR), as in its worked
//                     example.
//   * Loop predictor buffer (LPB) : 64 sets x 2 ways (128 entries),
//                     set index = IP[9:4], tag = IP[15:10].  Each entry holds
//                     {valid, 6-bit tag, 6-bit Count, 6-bit Limit,
//                     1-bit Prediction} plus LRU state, as in the paper,
//                     plus a 2-bit confidence counter (our addition, see
//                     below).
//   * Final outcome : confident LPB hit -> loop prediction
//                     else global hit   -> global counter
//                     else              -> bimodal counter
//                     The paper reports the opposite priority ("a global
//                     predictor hit overrides a loop predictor hit"); that
//                     order is available with loop_over_global = 0, but it
//                     lets the global entry for an all-taken history mispredict
//                     the exit of every loop longer than the history, so the
//                     LPB then gains almost nothing (REPORT.md, section 5).
//
// Loop predictor operation.  An entry describes a branch that goes in its
// "Prediction" direction Limit times in a row and then once the other way
// (the loop exit), over and over.  Count = how many Prediction-direction
// outcomes have been seen since the last exit.
//   * predict : Count == Limit ? !Prediction : Prediction
//   * update  : outcome == Prediction -> Count++ (if Count would pass 63 the
//                loop is too long for 6 bits: the entry becomes untrained);
//               outcome != Prediction -> this was an exit: if Count == Limit
//                the trip count repeated, confidence++; otherwise
//                Limit = Count, confidence = 0.  Count = 0.
//                Two exits in a row (Count == 0) mean the entry is oriented
//                the wrong way, so Prediction is flipped and it retrains.
//   * allocate: on an LPB miss when the bimodal prediction was wrong, i.e. on
//               the "first opposite outcome" (paper).  Prediction is set to
//               the bimodal direction (the direction that was just
//               contradicted), Count = 0, Limit unknown.  Victim = LRU way.
//   * use     : the loop prediction is used only when the entry's trip count
//               has repeated at least loop_conf times in a row (default 3).
//               The paper lists no confidence field; without it the LPB
//               overrides the bimodal table for every non-loop branch it
//               captures, which costs accuracy (see REPORT.md, section 5).
//   * The real LPB hit also requires a BTB hit; the BTB is not modelled here,
//     so the BTB is treated as always hitting.
//
// Update (resolved at retirement, i.e. in update()):
//   * The bimodal counter is always trained.
//   * The LPB entry (if any) is trained as above.
//   * On a global hit, the hit entry's counter is trained and the entry
//     becomes MRU.
//   * On a global miss, a new entry is allocated in the LRU way of the set
//     only if the prediction that was used without the global table (loop if
//     it was used, otherwise bimodal) was wrong, and its counter starts
//     weakly biased toward the actual outcome.  (The handout's worked example
//     allocates on every miss and keeps the victim's old counter; that
//     behaviour is available via pm_config::alloc_policy / alloc_counter and is
//     verified by test/test_example.cc.  REPORT.md compares the two.)
//   * The GHR shifts in the outcome of every conditional branch.
//   * Only conditional branches touch the predictor tables; other branches
//     are predicted "taken" (they are not scored by the simulator).
//   * Branch target prediction is not implemented (assignment says it can be
//     skipped), so target_prediction is always 0.
//
// All sizes and policies live in pm_config so the same class can be run
// at Pentium M scale (default constructor, used by predict.cc) and at the
// tiny scale of the assignment's worked example (see test/test_example.cc),
// and so the experiment driver (src/experiments.cc) can sweep the design
// space.  Set use_loop = 0 for the bimodal + global predictor only.
//
// Both set-associative tables use a small hand-written LRU cache; it was
// written from scratch for this assignment (no external cache code used).
// ---------------------------------------------------------------------------

#include <vector>

// hash functions for the global predictor
#define PM_HASH_GSHARE   0  // HASH = (IP >> addr_shift) ^ GHR  (worked example style)
#define PM_HASH_PM_GHR   1  // Pentium M bit selection, GHR in place of PIR:
                            //   index = IP[12:4] ^ H[14:6], tag = IP[18:13] ^ H[5:0]
#define PM_HASH_PM_PIR   2  // as above, H = real Pentium M path register (PIR)

// when to allocate a global entry on a global miss
#define PM_ALLOC_ALWAYS         0  // every miss (worked example behaviour)
#define PM_ALLOC_BIMODAL_WRONG  1  // only when the bimodal prediction was wrong
#define PM_ALLOC_BASE_WRONG     2  // only when the non-global prediction
                                   // (loop if used, else bimodal) was wrong

// counter value given to a newly allocated global entry
#define PM_CTR_KEEP   0  // keep the victim's old counter and train it (worked example)
#define PM_CTR_WEAK   1  // weakly biased toward the actual outcome (2 if T, 1 if NT)

#define PM_LOOP_UNKNOWN 0xff  // Limit not learned yet

struct pm_config {
	int bimodal_bits;   // log2(#bimodal counters)
	int addr_shift;     // low IP bits ignored (0 for x86; 2 in the example)
	int hist_bits;      // width of GHR / HASH
	int set_bits;       // log2(#sets) ; tag bits = hist_bits - set_bits
	int ways;           // associativity of the global table (2 or 4)
	int hash_mode;      // PM_HASH_*
	int alloc_policy;   // PM_ALLOC_*
	int alloc_counter;  // PM_CTR_*
	int bimodal_init;   // initial bimodal counter value (0..3)

	// loop predictor
	int use_loop;          // 1 = include the LPB
	int loop_set_bits;     // log2(#LPB sets)         (6 -> 64 sets)
	int loop_ways;         // LPB associativity       (2)
	int loop_shift;        // set index = IP[shift+set_bits-1 : shift]  (4)
	int loop_tag_bits;     // tag = next loop_tag_bits IP bits (6)
	int loop_count_bits;   // width of Count / Limit (6 -> trip counts up to 63)
	int loop_conf;         // repeats of the trip count needed before use (0..3)
	int loop_over_global;  // 1 = a confident LPB hit overrides a global hit

	pm_config (void) :
		bimodal_bits(12), addr_shift(0), hist_bits(15), set_bits(9),
		ways(4), hash_mode(PM_HASH_GSHARE),
		alloc_policy(PM_ALLOC_BASE_WRONG), alloc_counter(PM_CTR_WEAK),
		bimodal_init(2),
		use_loop(1), loop_set_bits(6), loop_ways(2), loop_shift(4),
		loop_tag_bits(6), loop_count_bits(6), loop_conf(3),
		loop_over_global(1) {}
};

class pm_update : public branch_update {
public:
        unsigned int index;     // bimodal index
        unsigned int set;       // global set index
        unsigned int tag;       // global tag
        int hit_way;            // way that hit, or -1 on a global miss
        bool bimodal_pred;      // bimodal prediction
        bool global_pred;       // global prediction (valid only on hit)
        unsigned int lset;      // LPB set index
        unsigned int ltag;      // LPB tag
        int loop_way;           // LPB way that hit, or -1
        bool loop_pred;         // loop prediction (valid only on LPB hit)
        bool loop_used;         // LPB hit and confident
        int provider;           // 0 = bimodal, 1 = global, 2 = loop
};

class pm_predictor : public branch_predictor {
public:
        pm_update u;
        branch_info bi;
        pm_config cfg;

        unsigned int tag_bits, bim_mask, set_mask, tag_mask, hist_mask;
        unsigned int l_set_mask, l_tag_mask, l_count_max;
        unsigned int ghr;       // global outcome history
        unsigned int pir;       // path information register (PM_HASH_PM_PIR only)

        std::vector<unsigned char> bim;     // bimodal 2-bit counters
        // global table, entry (set, way) stored at set*ways + way
        std::vector<unsigned char> g_valid, g_ctr, g_age;
        std::vector<unsigned short> g_tag;
        // loop predictor buffer, entry (set, way) stored at set*loop_ways + way
        std::vector<unsigned char> l_valid, l_tag, l_count, l_limit, l_pred,
                                   l_conf, l_age;

        // statistics (conditional branches only)
        long long n_cond, n_ghit, n_ghit_correct, n_bim_used, n_bim_used_correct,
                  n_bim_correct, n_alloc, n_loop_used, n_loop_correct,
                  n_loop_alloc;

        pm_predictor (void) { init (pm_config ()); }
        pm_predictor (const pm_config & c) { init (c); }

        void init (const pm_config & c) {
		cfg = c;
		tag_bits  = cfg.hist_bits - cfg.set_bits;
		bim_mask  = (1u << cfg.bimodal_bits) - 1;
		set_mask  = (1u << cfg.set_bits) - 1;
		tag_mask  = (1u << tag_bits) - 1;
		hist_mask = (1u << cfg.hist_bits) - 1;
		ghr = pir = 0;
		bim.assign (1u << cfg.bimodal_bits, cfg.bimodal_init);
		unsigned int n = (1u << cfg.set_bits) * cfg.ways;
		g_valid.assign (n, 0);
		g_ctr.assign (n, 0);
		g_tag.assign (n, 0);
		g_age.assign (n, 0);
		// LRU ages form a permutation 0..ways-1 in every set (0 = MRU).
		// Way 0 starts as the LRU way, so ways fill in order 0,1,2,...
		for (unsigned int s = 0; s < (1u << cfg.set_bits); s++)
			for (int w = 0; w < cfg.ways; w++)
				g_age[s * cfg.ways + w] = cfg.ways - 1 - w;

		l_set_mask  = (1u << cfg.loop_set_bits) - 1;
		l_tag_mask  = (1u << cfg.loop_tag_bits) - 1;
		l_count_max = (1u << cfg.loop_count_bits) - 1;
		unsigned int ln = (1u << cfg.loop_set_bits) * cfg.loop_ways;
		l_valid.assign (ln, 0);
		l_tag.assign (ln, 0);
		l_count.assign (ln, 0);
		l_limit.assign (ln, PM_LOOP_UNKNOWN);
		l_pred.assign (ln, 0);
		l_conf.assign (ln, 0);
		l_age.assign (ln, 0);
		for (unsigned int s = 0; s < (1u << cfg.loop_set_bits); s++)
			for (int w = 0; w < cfg.loop_ways; w++)
				l_age[s * cfg.loop_ways + w] = cfg.loop_ways - 1 - w;

		n_cond = n_ghit = n_ghit_correct = n_bim_used = 0;
		n_bim_used_correct = n_bim_correct = n_alloc = 0;
		n_loop_used = n_loop_correct = n_loop_alloc = 0;
        }

        // compute global-table set index and tag for this branch
        void global_index (unsigned int ip, unsigned int &set, unsigned int &tag) {
		if (cfg.hash_mode == PM_HASH_GSHARE) {
			unsigned int h = ((ip >> cfg.addr_shift) ^ ghr) & hist_mask;
			set = (h >> tag_bits) & set_mask;
			tag = h & tag_mask;
		} else {
			unsigned int h = (cfg.hash_mode == PM_HASH_PM_PIR) ? pir : ghr;
			set = ((ip >> 4) ^ (h >> tag_bits)) & set_mask;
			tag = ((ip >> (4 + cfg.set_bits)) ^ h) & tag_mask;
		}
        }

        // ---- generic LRU helpers (ages: 0 = MRU, ways-1 = LRU) ----------
        static void lru_touch (unsigned char *age, int ways, int w) {
		for (int i = 0; i < ways; i++)
			if (age[i] < age[w]) age[i]++;
		age[w] = 0;
        }

        // victim = an invalid way if any (oldest first), otherwise the LRU way
        static int lru_victim (const unsigned char *age, const unsigned char *valid, int ways) {
		int best = 0, best_inv = -1;
		for (int i = 0; i < ways; i++) {
			if (age[i] > age[best]) best = i;
			if (!valid[i] && (best_inv < 0 || age[i] > age[best_inv]))
				best_inv = i;
		}
		return best_inv >= 0 ? best_inv : best;
        }

        // global-table wrappers (used by the tests)
        void touch (unsigned int s, int w) { lru_touch (&g_age[s * cfg.ways], cfg.ways, w); }
        int victim (unsigned int s) {
		return lru_victim (&g_age[s * cfg.ways], &g_valid[s * cfg.ways], cfg.ways);
        }

        static void train (unsigned char &c, bool taken) {
		if (taken) { if (c < 3) c++; }
		else       { if (c > 0) c--; }
        }

        branch_update *predict (branch_info & b) {
		bi = b;
		u.hit_way = -1;
		u.loop_way = -1;
		u.loop_used = false;
		u.provider = 0;
		if (b.br_flags & BR_CONDITIONAL) {
			// bimodal lookup
			u.index = (b.address >> cfg.addr_shift) & bim_mask;
			u.bimodal_pred = bim[u.index] >> 1;

			// global lookup (tag match in the selected set)
			global_index (b.address, u.set, u.tag);
			int base = u.set * cfg.ways;
			for (int w = 0; w < cfg.ways; w++)
				if (g_valid[base + w] && g_tag[base + w] == u.tag) {
					u.hit_way = w;
					break;
				}
			u.global_pred = (u.hit_way >= 0) ? (g_ctr[base + u.hit_way] >> 1) : false;

			// loop predictor lookup
			if (cfg.use_loop) {
				u.lset = (b.address >> cfg.loop_shift) & l_set_mask;
				u.ltag = (b.address >> (cfg.loop_shift + cfg.loop_set_bits)) & l_tag_mask;
				int lb = u.lset * cfg.loop_ways;
				for (int w = 0; w < cfg.loop_ways; w++)
					if (l_valid[lb + w] && l_tag[lb + w] == u.ltag) {
						u.loop_way = w;
						break;
					}
				if (u.loop_way >= 0) {
					int i = lb + u.loop_way;
					u.loop_pred = (l_count[i] == l_limit[i]) ? !l_pred[i] : l_pred[i];
					u.loop_used = l_limit[i] != PM_LOOP_UNKNOWN &&
					              l_conf[i] >= cfg.loop_conf;
				}
			}

			// choose the provider
			bool global_first = !(cfg.loop_over_global && u.loop_used);
			if (u.hit_way >= 0 && global_first) u.provider = 1;
			else if (u.loop_used)                u.provider = 2;
			else                                 u.provider = 0;

			// predict branch outcome
			u.direction_prediction (u.provider == 1 ? u.global_pred :
			                        u.provider == 2 ? u.loop_pred : u.bimodal_pred);
		} else {
			u.direction_prediction (true);
		}
		// predict branch target address (not implemented)
		u.target_prediction (0);
		return &u;
        }

        // train / allocate the loop predictor entry for this branch
        void update_loop (pm_update *p, bool taken) {
		int lb = p->lset * cfg.loop_ways;
		if (p->loop_way >= 0) {
			int i = lb + p->loop_way;
			if (taken == (bool) l_pred[i]) {
				// still inside the loop
				// Count can reach l_count_max + 1, which marks "too long
				// for the Count field": the entry stays untrained
				if (l_count[i] <= l_count_max) l_count[i]++;
				if (l_count[i] > l_count_max) { l_limit[i] = PM_LOOP_UNKNOWN; l_conf[i] = 0; }
				if (l_limit[i] != PM_LOOP_UNKNOWN && l_count[i] > l_limit[i])
					l_conf[i] = 0;                 // ran past the learned limit
			} else {
				// loop exit
				if (l_count[i] == 0) {
					// two "exits" in a row: the entry was oriented the wrong
					// way (e.g. allocated while the bimodal counter was still
					// cold); flip it so the current outcome is the loop body
					l_pred[i]  = taken;
					l_limit[i] = PM_LOOP_UNKNOWN;
					l_conf[i]  = 0;
					l_count[i] = 1;
				} else if (l_count[i] == l_limit[i]) {
					if (l_conf[i] < 3) l_conf[i]++;
					l_count[i] = 0;
				} else {
					l_limit[i] = (l_count[i] <= l_count_max) ? l_count[i] : PM_LOOP_UNKNOWN;
					l_conf[i] = 0;
					l_count[i] = 0;
				}
			}
			lru_touch (&l_age[lb], cfg.loop_ways, p->loop_way);
		} else if (p->bimodal_pred != taken) {
			// first opposite outcome: allocate
			int w = lru_victim (&l_age[lb], &l_valid[lb], cfg.loop_ways);
			int i = lb + w;
			l_valid[i] = 1;
			l_tag[i]   = p->ltag;
			l_pred[i]  = !taken;          // the direction that was just broken
			l_count[i] = 0;
			l_limit[i] = PM_LOOP_UNKNOWN;
			l_conf[i]  = 0;
			lru_touch (&l_age[lb], cfg.loop_ways, w);
			n_loop_alloc++;
		}
        }

        void update (branch_update *bu, bool taken, unsigned int target) {
		pm_update *p = (pm_update *) bu;
		if (bi.br_flags & BR_CONDITIONAL) {
			// statistics
			n_cond++;
			n_bim_correct += (p->bimodal_pred == taken);
			if (p->hit_way >= 0) {
				n_ghit++;
				n_ghit_correct += (p->global_pred == taken);
			}
			if (p->provider == 2) {
				n_loop_used++;
				n_loop_correct += (p->loop_pred == taken);
			} else if (p->provider == 0) {
				n_bim_used++;
				n_bim_used_correct += (p->bimodal_pred == taken);
			}

			// global predictor
			bool base_pred = p->loop_used ? p->loop_pred : p->bimodal_pred;
			int base = p->set * cfg.ways;
			if (p->hit_way >= 0) {
				train (g_ctr[base + p->hit_way], taken);
				touch (p->set, p->hit_way);
			} else if (cfg.alloc_policy == PM_ALLOC_ALWAYS ||
			           (cfg.alloc_policy == PM_ALLOC_BIMODAL_WRONG && p->bimodal_pred != taken) ||
			           (cfg.alloc_policy == PM_ALLOC_BASE_WRONG && base_pred != taken)) {
				int w = victim (p->set);
				g_valid[base + w] = 1;
				g_tag[base + w] = p->tag;
				if (cfg.alloc_counter == PM_CTR_WEAK)
					g_ctr[base + w] = taken ? 2 : 1;
				else
					train (g_ctr[base + w], taken);
				touch (p->set, w);
				n_alloc++;
			}

			// loop predictor
			if (cfg.use_loop) update_loop (p, taken);

			// bimodal predictor
			train (bim[p->index], taken);

			// global history: shift in the outcome
			ghr = ((ghr << 1) | (taken ? 1 : 0)) & hist_mask;

			// Pentium M PIR (ISPASS'09 eq. 3): taken conditional branches
			if (taken)
				pir = ((pir << 2) ^ ((bi.address >> 4) & 0x7fff)) & 0x7fff;
		} else if ((bi.br_flags & BR_INDIRECT) && !(bi.br_flags & BR_RETURN)) {
			// Pentium M PIR: indirect branches mix in IP[18:10] and target[5:0]
			unsigned int v = (((bi.address >> 10) & 0x1ff) << 6) | (target & 0x3f);
			pir = ((pir << 2) ^ v) & 0x7fff;
		}
        }

};

//
// Complete Pentium M branch predictors for extra credit
// This class implements the complete Pentium M branch prediction units. 
// It implements both branch target prediction and branch outcome predicton. 
class cpm_update : public branch_update {
public:
        unsigned int index;
};

class cpm_predictor : public branch_predictor {
public:
        cpm_update u;

        cpm_predictor (void) {
        }

        branch_update *predict (branch_info & b) {
            u.direction_prediction (true);
            u.target_prediction (0);
            return &u;
        }

        void update (branch_update *u, bool taken, unsigned int target) {
        }

};


