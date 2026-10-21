Per-trace MPKI

| Trace | Bimodal only | gshare 32K | Bimodal + global | **+ loop (submitted)** | Change from loop |
|---|---:|---:|---:|---:|---:|
| 164.gzip | 15.031 | 12.473 | 12.439 | **11.719** | -5.8% |
| 175.vpr | 14.673 | 13.415 | 13.298 | **13.141** | -1.2% |
| 176.gcc | 19.313 | 11.254 | 9.266 | **9.169** | -1.0% |
| 181.mcf | 33.271 | 15.837 | 15.624 | **15.861** | +1.5% |
| 186.crafty | 13.097 | 5.837 | 5.208 | **5.199** | -0.2% |
| 197.parser | 15.420 | 10.008 | 8.542 | **8.621** | +0.9% |
| 201.compress | 9.368 | 7.831 | 7.688 | **7.117** | -7.4% |
| 202.jess | 8.773 | 1.562 | 1.124 | **0.974** | -13.3% |
| 205.raytrace | 3.704 | 2.756 | 2.293 | **2.201** | -4.0% |
| 209.db | 6.437 | 3.909 | 3.608 | **3.463** | -4.0% |
| 213.javac | 3.198 | 2.267 | 2.052 | **1.649** | -19.6% |
| 222.mpegaudio | 3.166 | 2.188 | 2.031 | **1.617** | -20.4% |
| 227.mtrt | 3.682 | 2.657 | 2.389 | **2.285** | -4.4% |
| 228.jack | 7.175 | 3.033 | 2.349 | **2.151** | -8.4% |
| 252.eon | 9.443 | 1.807 | 1.506 | **1.210** | -19.7% |
| 253.perlbmk | 5.848 | 2.554 | 1.774 | **1.674** | -5.6% |
| 254.gap | 9.760 | 3.926 | 3.509 | **3.484** | -0.7% |
| 255.vortex | 1.930 | 1.222 | 0.830 | **0.835** | +0.6% |
| 256.bzip2 | 0.113 | 0.094 | 0.062 | **0.051** | -17.7% |
| 300.twolf | 21.943 | 21.489 | 21.390 | **20.568** | -3.8% |
| **Average** | 10.267 | 6.306 | 5.849 | **5.649** | -3.4% |

Loop predictor usage (submitted predictor)

| Trace | Branches predicted by the LPB | LPB accuracy |
|---|---:|---:|
| 164.gzip | 9.1% | 95.0% |
| 175.vpr | 10.2% | 98.0% |
| 176.gcc | 8.6% | 97.6% |
| 181.mcf | 13.1% | 95.4% |
| 186.crafty | 4.0% | 97.1% |
| 197.parser | 6.3% | 94.8% |
| 201.compress | 5.3% | 98.5% |
| 202.jess | 16.1% | 99.7% |
| 205.raytrace | 1.7% | 97.4% |
| 209.db | 3.9% | 99.2% |
| 213.javac | 5.3% | 99.0% |
| 222.mpegaudio | 5.3% | 99.0% |
| 227.mtrt | 2.2% | 98.0% |
| 228.jack | 5.4% | 98.5% |
| 252.eon | 58.8% | 99.8% |
| 253.perlbmk | 4.7% | 98.0% |
| 254.gap | 20.8% | 97.0% |
| 255.vortex | 3.9% | 99.3% |
| 256.bzip2 | 0.2% | 99.8% |
| 300.twolf | 12.8% | 98.4% |
| **Average** | 9.9% | 98.0% |

Bimodal + global only (no loop predictor): component statistics

| Trace | Global hit rate | Global accuracy on hits | Bimodal accuracy when used |
|---|---:|---:|---:|
| 164.gzip | 50.0% | 86.5% | 97.1% |
| 175.vpr | 54.9% | 85.5% | 94.6% |
| 176.gcc | 40.6% | 91.1% | 96.9% |
| 181.mcf | 53.4% | 90.9% | 94.9% |
| 186.crafty | 40.0% | 92.3% | 95.4% |
| 197.parser | 47.1% | 90.3% | 97.3% |
| 201.compress | 47.6% | 86.3% | 99.9% |
| 202.jess | 45.4% | 98.6% | 99.7% |
| 205.raytrace | 31.3% | 94.6% | 99.7% |
| 209.db | 34.2% | 92.3% | 99.9% |
| 213.javac | 31.4% | 95.5% | 99.8% |
| 222.mpegaudio | 40.2% | 96.4% | 99.8% |
| 227.mtrt | 44.8% | 96.1% | 99.7% |
| 228.jack | 35.4% | 95.2% | 99.6% |
| 252.eon | 38.1% | 95.2% | 99.8% |
| 253.perlbmk | 35.9% | 96.4% | 100.0% |
| 254.gap | 39.8% | 93.9% | 99.8% |
| 255.vortex | 31.9% | 97.9% | 99.9% |
| 256.bzip2 | 46.2% | 100.0% | 100.0% |
| 300.twolf | 36.7% | 75.3% | 88.5% |

All configurations

| Configuration | Avg MPKI | vs. submitted |
|---|---:|---:|
| LPB 512 entries (256 x 2) (`lp_512entries`) | 5.571 | -0.078 |
| LPB index IP[5:0] instead of IP[9:4] (`lp_index_ip5_0`) | 5.604 | -0.045 |
| LPB 256 entries (128 x 2) (`lp_256entries`) | 5.614 | -0.036 |
| LPB 128 entries, 4-way (32 x 4) (`lp_128_4way`) | 5.635 | -0.014 |
| 8-bit Count/Limit (loops up to 255) (`lp_count8bit`) | 5.647 | -0.002 |
| Global allocates on bimodal (not final) miss (`lp_galloc_bimodal_wrong`) | 5.649 | -0.000 |
| Loop first, 3 repeats (= submitted) (`lp_loopfirst_conf3`) | 5.649 | +0.000 |
| Submitted: loop first, 3 repeats, 128 entries (`pm_loop`) | 5.649 | +0.000 |
| Loop first, 2 repeats (`lp_loopfirst_conf2`) | 5.675 | +0.026 |
| LPB 64 entries (32 x 2) (`lp_64entries`) | 5.707 | +0.058 |
| PM: 8-way, 512 sets (4K entries) (`pm_8way_512set`) | 5.756 | +0.107 |
| Global first, 3 repeats (`lp_globalfirst_conf3`) | 5.814 | +0.164 |
| Global first, 2 repeats (`lp_globalfirst_conf2`) | 5.815 | +0.166 |
| Global first, 1 repeat (`lp_globalfirst_conf1`) | 5.836 | +0.187 |
| No loop predictor (`pm_noloop`) | 5.849 | +0.200 |
| Loop first, 1 repeat (`lp_loopfirst_conf1`) | 5.855 | +0.206 |
| PM: 16-bit history (1024 x 2) (`pm_h16_1024x2`) | 5.868 | +0.219 |
| PM: paper's IP bits + GHR (`pm_hash_pmbits_ghr`) | 5.922 | +0.273 |
| PM: 2-way, 1024 sets (2K entries) (`pm_2way_1024set`) | 5.928 | +0.278 |
| PM: keep victim counter on allocate (`pm_ctr_keep`) | 5.936 | +0.287 |
| PM: 17-bit history (2048 x 1) (`pm_h17_2048x1`) | 6.021 | +0.372 |
| Global first (paper order), 0 repeats (`lp_globalfirst_conf0`) | 6.112 | +0.462 |
| PM: 2-way, 512 sets (1K entries) (`pm_2way_512set`) | 6.134 | +0.485 |
| PM: 14-bit history (256 x 8) (`pm_h14_256x8`) | 6.135 | +0.486 |
| gshare 32K (8 KB, sample) (`gshare_32K_h15`) | 6.306 | +0.657 |
| PM: allocate on every global miss (`pm_alloc_always`) | 6.607 | +0.958 |
| PM: direct-mapped, 512 sets (`pm_1way_512set`) | 6.732 | +1.082 |
| PM: 13-bit history (128 x 16) (`pm_h13_128x16`) | 6.814 | +1.164 |
| gshare 16K (4 KB) (`gshare_16K_h14`) | 6.821 | +1.172 |
| PM: handout policy (alloc every miss, keep ctr) (`pm_example_policy`) | 7.762 | +2.112 |
| PM: 12-bit history (64 sets x 32 ways) (`pm_h12_64x32`) | 8.328 | +2.679 |
| Loop first, use immediately (0 repeats) (`lp_loopfirst_conf0`) | 8.361 | +2.711 |
| PM: paper's IP bits + PIR path history (`pm_hash_pmbits_pir`) | 8.934 | +3.285 |
| gshare 4K (1 KB) (`gshare_4K_h12`) | 9.006 | +3.357 |
| Bimodal only, 4K entries (`bimodal_4K`) | 10.267 | +4.618 |
| Always predict taken (`always_taken`) | 86.312 | +80.662 |
