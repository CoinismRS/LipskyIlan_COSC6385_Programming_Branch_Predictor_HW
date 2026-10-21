# Pentium M Hybrid Branch Predictor (Assignment 1)

**GitHub repository:** <https://github.com/CoinismRS/LipskyIlan_COSC6385_Programming_Branch_Predictor_HW>

This repository is a C++ implementation of the branch-direction predictor
used in the Intel Pentium M. It has three parts:

* a **4096-entry bimodal table**;
* a **tagged 4-way, 512-set global predictor**;
* a **128-entry loop predictor**.

A confident loop entry wins first, then a global hit, then the bimodal
table. The predictor is built on the CBP-2 branch prediction simulation
infrastructure.

**Result:** 5.649 average MPKI over the 20 CBP-2 traces, or 5.849 without
the loop predictor. For comparison, the bimodal table alone gets 10.267, and
the kit's 32K-entry gshare gets 6.306 while using about twice the storage. See [REPORT.md](REPORT.md) (also included as
`REPORT.pdf`) for the design, verification and results.

## Repository layout

```
.
├── README.md                      this file
├── REPORT.md / REPORT.pdf         report: what was done + results
├── figures/                       charts used in the report
├── results/
│   ├── run_output.txt             output of ./run.sh traces (submitted predictor)
│   ├── experiments.csv            all 20 traces x 36 predictor configurations
│   ├── summary_tables.md          tables generated from experiments.csv
│   └── test_output.txt            output of make test (both unit tests)
├── tools/make_figures.py          regenerates figures/ and summary_tables.md
└── cbp2-infrastructure-v2/        simulation kit (original files + my changes)
    ├── run                        original csh driver (needs csh + dc)
    ├── run.sh                     bash equivalent of run          [added]
    ├── run_experiments.sh         design-space sweep -> results/   [added]
    ├── test/test_example.cc       replays the handout's worked example [added]
    ├── test/test_loop.cc          loop predictor unit tests        [added]
    ├── traces/                    20 CBP-2 traces (.bz2)
    └── src/
        ├── my_predictor.h         pm_predictor implementation      [modified]
        ├── experiments.cc         multi-predictor experiment driver [added]
        ├── Makefile               added experiments / test targets  [modified]
        └── predict.cc, trace.cc, ...  unchanged kit files
```

## Requirements

* Linux or macOS with `g++` (any C++98 or newer compiler) and `make`.
* `bzip2` at `/usr/bin/bzip2`, which the kit uses to read the traces. If
  yours is elsewhere, edit `BZCAT` in `src/trace.h`.
* Optional: `csh` + `dc` to use the original `run` script. `run.sh` needs
  only bash and awk.
* Optional: Python 3 + matplotlib to regenerate the figures.

## Build and run

```bash
cd cbp2-infrastructure-v2/src
make                      # builds predict (and predict_extra_credit)
./predict ../traces/164.gzip/gzip.trace.bz2     # -> 11.719 MPKI

cd ..
./run.sh traces           # all 20 traces + average  (or: ./run traces  with csh)
```

Expected end of the output:

```
traces/300.twolf/twolf.trace.bz2        	20.568
average MPKI: 5.649
```

## Tests

```bash
cd cbp2-infrastructure-v2/src
make test
```

This runs two programs:

* **`test_example`** runs `pm_predictor` at the handout's scale with the
  loop predictor off: 16 local counters, a 4-bit GHR, and a 4-row x 2-way
  global table with 2-bit tags. It uses the handout's initial values and
  replays the 22-branch worked example. Every column of the "Hybrid Predictor
  Results" table and two full snapshots of the global table must match.
* **`test_loop`** runs 27 checks on the loop predictor:
    * trip counts 1–63 learned exactly;
    * not-taken loops;
    * loops too long for 6 bits are never used;
    * re-learning after a trip-count change;
    * index/tag fields and LRU replacement;
    * an end-to-end loop with zero exit mispredictions.

## Reproduce the experiments and figures

```bash
cd cbp2-infrastructure-v2
./run_experiments.sh          # ~4 min; writes ../results/experiments.csv
cd ..
python3 tools/make_figures.py # writes figures/*.png and results/summary_tables.md
```

## Configuration

All parameters live in `struct pm_config` in `src/my_predictor.h`. The
default constructor is the submitted configuration:

| Field | Default | Meaning |
|---|---|---|
| `bimodal_bits` | 12 | 4096 bimodal counters, index IP[11:0] |
| `hist_bits` | 15 | GHR / hash width |
| `set_bits` | 9 | 512 sets (tag = 15 − 9 = 6 bits) |
| `ways` | 4 | associativity (2 = the "two-way" option in the handout) |
| `hash_mode` | `PM_HASH_GSHARE` | HASH = IP ^ GHR (also `PM_HASH_PM_GHR`, `PM_HASH_PM_PIR`) |
| `alloc_policy` | `PM_ALLOC_BASE_WRONG` | allocate a global entry only when the loop/bimodal prediction was wrong (`PM_ALLOC_ALWAYS` = handout example) |
| `alloc_counter` | `PM_CTR_WEAK` | new entry counter = weakly toward outcome (`PM_CTR_KEEP` = handout example) |
| `use_loop` | 1 | 0 = bimodal + global only (5.849 MPKI) |
| `loop_set_bits`, `loop_ways` | 6, 2 | LPB = 64 sets x 2 ways = 128 entries |
| `loop_shift`, `loop_tag_bits` | 4, 6 | LPB set = IP[9:4], tag = IP[15:10] |
| `loop_count_bits` | 6 | Count/Limit width (loops up to 63 iterations) |
| `loop_conf` | 3 | trip-count repeats required before the LPB is used |
| `loop_over_global` | 1 | 1 = confident LPB hit overrides a global hit (0 = the paper's order) |

To try a different configuration with the stock `predict` binary, change
the defaults in the `pm_config` constructor and run `make` again.

## Scope

This implements branch **outcome** prediction: the required bimodal and
global predictors, plus the optional loop predictor. Branch target
prediction (BTB/iBTB) is not implemented, as the assignment allows.
`target_prediction` is always 0, and the extra-credit `cpm_predictor` is
left as the kit's stub. Because there is no BTB, the "LPB hit requires a
BTB hit" condition is treated as always true. The PIR is replaced by a global outcome-history
register, which the assignment also allows. A PIR variant is included as an
experiment.

## Getting the code

```bash
git clone https://github.com/CoinismRS/LipskyIlan_COSC6385_Programming_Branch_Predictor_HW.git
```

Build outputs are excluded by `.gitignore`.

## Credits

* CBP-2 simulation infrastructure, traces and the sample gshare predictor:
  the course kit (`cbp2-infrastructure-v2`).
* Pentium M predictor organization: V. Uzelac and A. Milenkovic, ISPASS 2009.
* All predictor, cache (LRU) and test code in this repository was written
  for this assignment. No external cache implementation was used.
