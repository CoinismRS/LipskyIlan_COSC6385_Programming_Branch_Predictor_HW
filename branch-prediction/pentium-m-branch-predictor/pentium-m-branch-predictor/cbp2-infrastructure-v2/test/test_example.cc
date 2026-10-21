// test_example.cc
// Replays the "Hybrid Predictor Example" from the assignment handout
// (programming-branch-predictor.pdf, Appendix B) through pm_predictor and
// checks every column of the "Hybrid Predictor Results" table, the local
// (bimodal) counter read at each step, and full snapshots of the global
// predictor table before steps 12 and 22.
//
// Example configuration: 8-bit addresses, 16 local 2-bit counters indexed
// by IP[5:2], 4-bit GHR, global predictor of 4 rows x 2 ways with 2-bit tags,
// HASH = IP[5:2] xor GHR, row = HASH[3:2], tag = HASH[1:0].
// Initial values as in the handout: local counter i = i mod 4, global way0
// counter in row r = r, way1 counter = 3 - r, way0 is the LRU way.
// The handout allocates a global entry on every global miss and keeps the
// victim's old counter value (training it with the outcome).
//
// Build & run:  cd src && make test

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "branch.h"
#include "predictor.h"
#include "my_predictor.h"

struct step {
	const char *name; unsigned int addr; int taken;
	const char *ghr, *hash; int row; const char *tag;
	int local_ctr; char local; const char *global; char final_;
};

// Columns copied from the handout tables.
// global: "-" = no hit, "T"/"N" = hit with that prediction.
static const step steps[] = {
	{"b1",0x44,1,"0000","0001",0,"01",1,'N',"-",'N'},
	{"b3",0x90,1,"0001","0101",1,"01",0,'N',"-",'N'},
	{"b1",0x44,0,"0011","0010",0,"10",2,'T',"-",'T'},
	{"b2",0x6C,0,"0110","1101",3,"01",3,'T',"-",'T'},
	{"b3",0x90,1,"1100","1000",2,"00",1,'N',"-",'N'},
	{"b1",0x44,1,"1001","1000",2,"00",1,'N',"T",'T'},
	{"b3",0x90,1,"0011","0111",1,"11",2,'T',"-",'T'},
	{"b1",0x44,0,"0111","0110",1,"10",2,'T',"-",'T'},
	{"b2",0x6C,0,"1110","0101",1,"01",2,'T',"-",'T'},
	{"b3",0x90,1,"1100","1000",2,"00",3,'T',"T",'T'},
	{"b1",0x44,0,"1001","1000",2,"00",1,'N',"T",'T'},
	{"b2",0x6C,1,"0010","1001",2,"01",1,'N',"-",'N'},
	{"b1",0x44,0,"0101","0100",1,"00",0,'N',"-",'N'},
	{"b2",0x6C,0,"1010","0001",0,"01",2,'T',"N",'N'},
	{"b3",0x90,1,"0100","0000",0,"00",3,'T',"-",'T'},
	{"b1",0x44,1,"1001","1000",2,"00",0,'N',"T",'T'},
	{"b3",0x90,1,"0011","0111",1,"11",3,'T',"-",'T'},
	{"b1",0x44,0,"0111","0110",1,"10",1,'N',"-",'N'},
	{"b2",0x6C,0,"1110","0101",1,"01",1,'N',"-",'N'},
	{"b3",0x90,1,"1100","1000",2,"00",3,'T',"T",'T'},
	{"b1",0x44,1,"1001","1000",2,"00",0,'N',"T",'T'},
	{"b3",0x90,1,"0011","0111",1,"11",3,'T',"-",'T'},
};

// Global table snapshots from the handout: per row {valid, tag, 2bc, LRU}
// for way0 then way1.
struct gentry { int valid, tag, ctr, lru; };
static const gentry snap12[4][2] = {
	{{1,1,1,1},{1,2,2,0}},
	{{1,2,1,1},{1,1,2,0}},
	{{1,0,2,0},{0,0,1,1}},
	{{1,1,2,0},{0,0,0,1}},
};
static const gentry snap22[4][2] = {
	{{1,1,0,1},{1,0,3,0}},
	{{1,2,0,1},{1,1,2,0}},
	{{1,0,3,0},{1,1,2,1}},
	{{1,1,2,0},{0,0,0,1}},
};

static int failures = 0;

static void bits (unsigned int v, int n, char *out) {
	for (int i = 0; i < n; i++) out[i] = ((v >> (n - 1 - i)) & 1) ? '1' : '0';
	out[n] = 0;
}

static void check (bool ok, int stepno, const char *what) {
	if (!ok) { printf ("  MISMATCH at step %d: %s\n", stepno, what); failures++; }
}

static void check_snapshot (pm_predictor &p, const gentry s[4][2], int stepno) {
	for (int r = 0; r < 4; r++)
		for (int w = 0; w < 2; w++) {
			int i = r * 2 + w;
			const gentry &e = s[r][w];
			char msg[128];
			snprintf (msg, sizeof msg, "global row %d way %d", r, w);
			check (p.g_valid[i] == e.valid && p.g_tag[i] == e.tag &&
			       p.g_ctr[i] == e.ctr && p.g_age[i] == e.lru, stepno, msg);
		}
}

int main (void) {
	pm_config c;
	c.bimodal_bits  = 4;
	c.addr_shift    = 2;
	c.hist_bits     = 4;
	c.set_bits      = 2;
	c.ways          = 2;
	c.hash_mode     = PM_HASH_GSHARE;
	c.alloc_policy  = PM_ALLOC_ALWAYS;
	c.alloc_counter = PM_CTR_KEEP;
	c.use_loop      = 0;      // the handout example has no loop predictor
	pm_predictor p (c);

	// handout's initial values
	for (int i = 0; i < 16; i++) p.bim[i] = i % 4;
	for (int r = 0; r < 4; r++) { p.g_ctr[r * 2] = r; p.g_ctr[r * 2 + 1] = 3 - r; }

	printf ("step br  addr  out  GHR  hash row tag  lctr local global final\n");
	int n = sizeof steps / sizeof steps[0];
	for (int k = 0; k < n; k++) {
		const step &s = steps[k];
		int stepno = k + 1;
		if (stepno == 12) check_snapshot (p, snap12, stepno);
		if (stepno == 22) check_snapshot (p, snap22, stepno);

		branch_info bi;
		bi.address = s.addr; bi.opcode = 0; bi.br_flags = BR_CONDITIONAL;
		char ghr[8], hash[8], tag[8];
		bits (p.ghr, 4, ghr);
		bits ((s.addr >> 2) ^ p.ghr, 4, hash) ;
		pm_update *u = (pm_update *) p.predict (bi);
		bits (u->tag, 2, tag);
		int lctr = p.bim[u->index];
		char local = u->bimodal_pred ? 'T' : 'N';
		const char *global = u->hit_way < 0 ? "-" : (u->global_pred ? "T" : "N");
		char fin = u->direction_prediction () ? 'T' : 'N';

		printf ("%4d %s  0x%02X  %s   %s %s %3d  %s  %4d %5c %6s %5c\n",
		        stepno, s.name, s.addr, s.taken ? "T " : "NT", ghr, hash,
		        u->set, tag, lctr, local, global, fin);

		check (strcmp (ghr, s.ghr) == 0, stepno, "GHR");
		check (strcmp (hash, s.hash) == 0, stepno, "hash");
		check ((int) u->set == s.row, stepno, "row");
		check (strcmp (tag, s.tag) == 0, stepno, "tag");
		check (lctr == s.local_ctr, stepno, "local counter");
		check (local == s.local, stepno, "local prediction");
		check (strcmp (global, s.global) == 0, stepno, "global prediction");
		check (fin == s.final_, stepno, "final prediction");

		p.update (u, s.taken, 0);
	}

	if (failures == 0)
		printf ("\nPASS: all %d steps and both global-table snapshots match the handout.\n", n);
	else
		printf ("\nFAIL: %d mismatches\n", failures);
	return failures ? 1 : 0;
}
