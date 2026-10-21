#!/usr/bin/env python3
"""Builds the report figures and summary tables from results/experiments.csv.
Usage: python3 tools/make_figures.py   (run from the repository root)"""
import csv, collections, os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

SURF, INK, INK2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3df"
BLUE, ORANGE, AQUA, GRAY = "#2a78d6", "#eb6834", "#1baf7a", "#b9b8b2"
plt.rcParams.update({"font.size": 10, "axes.edgecolor": GRID, "axes.labelcolor": INK2,
    "xtick.color": INK2, "ytick.color": INK2, "figure.facecolor": SURF, "axes.facecolor": SURF,
    "axes.spines.top": False, "axes.spines.right": False, "font.family": "DejaVu Sans"})

rows = list(csv.DictReader(open("results/experiments.csv")))
mpki = {(r["trace"], r["predictor"]): float(r["mpki"]) for r in rows}
traces = sorted({r["trace"] for r in rows}, key=lambda t: int(t.split(".")[0]))
preds = []
for r in rows:
    if r["predictor"] not in preds: preds.append(r["predictor"])
avg = {p: sum(mpki[(t, p)] for t in traces) / len(traces) for p in preds}
stat = collections.defaultdict(lambda: collections.defaultdict(list))
for r in rows:
    for k in ("global_hit_rate", "global_hit_accuracy", "bimodal_used_accuracy"):
        if r[k]: stat[r["predictor"]][k].append(float(r[k]))

# ---- Figure 1: per-trace MPKI ---------------------------------------------
series = [("bimodal_4K", "Bimodal only (4K x 2-bit, 1 KB)", GRAY),
          ("gshare_32K_h15", "gshare 32K (sample code, 8 KB)", ORANGE),
          ("pm_noloop", "Bimodal + global (~3.75 KB)", AQUA),
          ("pm_loop", "Bimodal + global + loop (submitted, ~4.1 KB)", BLUE)]
fig, ax = plt.subplots(figsize=(8, 12))
h = 0.2
for i, (p, label, col) in enumerate(series):
    ys = [len(traces) - 1 - k + (1.5 - i) * h for k in range(len(traces))]
    ax.barh(ys, [mpki[(t, p)] for t in traces], height=h - 0.03, color=col, label=label)
ax.set_yticks(range(len(traces)))
ax.set_yticklabels(list(reversed(traces)))
ax.set_xlabel("Mispredictions per 1000 instructions (MPKI) - lower is better")
ax.xaxis.grid(True, color=GRID, lw=0.8); ax.set_axisbelow(True)
ax.legend(loc="upper center", bbox_to_anchor=(0.45, -0.05), frameon=False, labelcolor=INK, ncol=1)
ax.set_title("MPKI per trace", loc="left", color=INK, fontsize=12, fontweight="bold")
ax.set_ylim(-0.6, len(traces) - 0.4)
fig.tight_layout(); fig.savefig("figures/mpki_per_trace.png", dpi=150)

# ---- Figure 2: average MPKI for every configuration -----------------------
labels = {
 "bimodal_4K": "Bimodal only, 4K entries",
 "gshare_4K_h12": "gshare 4K (1 KB)",
 "gshare_16K_h14": "gshare 16K (4 KB)",
 "gshare_32K_h15": "gshare 32K (8 KB, sample)",
 "pm_noloop": "PM bimodal + global (4-way, 512 sets)",
 "pm_loop": "PM bimodal + global + loop (submitted)",
 "pm_example_policy": "PM: handout policy (alloc every miss, keep ctr)",
 "pm_alloc_always": "PM: allocate on every global miss",
 "pm_ctr_keep": "PM: keep victim counter on allocate",
 "pm_2way_512set": "PM: 2-way, 512 sets (1K entries)",
 "pm_2way_1024set": "PM: 2-way, 1024 sets (2K entries)",
 "pm_1way_512set": "PM: direct-mapped, 512 sets",
 "pm_8way_512set": "PM: 8-way, 512 sets (4K entries)",
 "pm_hash_pmbits_ghr": "PM: paper's IP bits + GHR",
 "pm_hash_pmbits_pir": "PM: paper's IP bits + PIR path history",
 "pm_h12_64x32": "PM: 12-bit history (64 sets x 32 ways)",
 "pm_h13_128x16": "PM: 13-bit history (128 x 16)",
 "pm_h14_256x8": "PM: 14-bit history (256 x 8)",
 "pm_h16_1024x2": "PM: 16-bit history (1024 x 2)",
 "pm_h17_2048x1": "PM: 17-bit history (2048 x 1)",
}
order = sorted([p for p in preds if p != "always_taken" and not p.startswith("lp_") and p != "pm_loop"], key=lambda p: avg[p])
fig, ax = plt.subplots(figsize=(8, 7))
ys = list(range(len(order)))[::-1]
cols = [BLUE if p == "pm_noloop" else GRAY for p in order]
ax.barh(ys, [avg[p] for p in order], color=cols, height=0.7)
for y, p in zip(ys, order):
    ax.text(avg[p] + 0.08, y, f"{avg[p]:.3f}", va="center", color=INK if p == "pm_noloop" else INK2,
            fontsize=9, fontweight="bold" if p == "pm_noloop" else "normal")
ax.set_yticks(ys); ax.set_yticklabels([labels[p] for p in order])
ax.set_xlabel("Average MPKI over 20 traces - lower is better")
ax.xaxis.grid(True, color=GRID, lw=0.8); ax.set_axisbelow(True)
ax.set_xlim(0, max(avg[p] for p in order) * 1.12)
fig.suptitle("Bimodal + global design space (average MPKI, no loop predictor)", x=0.02, ha="left", color=INK, fontsize=12, fontweight="bold")
fig.tight_layout(); fig.savefig("figures/design_space.png", dpi=150)

# ---- Figure 3: history length at constant 2048 global entries -------------
hist = [("pm_h12_64x32", 12), ("pm_h13_128x16", 13), ("pm_h14_256x8", 14),
        ("pm_noloop", 15), ("pm_h16_1024x2", 16), ("pm_h17_2048x1", 17)]
fig, ax = plt.subplots(figsize=(7, 3.8))
xs = [b for _, b in hist]; vs = [avg[p] for p, _ in hist]
ax.plot(xs, vs, color=BLUE, lw=2, marker="o", ms=8, markeredgecolor=SURF, markeredgewidth=2)
for x, v in zip(xs, vs):
    ax.annotate(f"{v:.2f}", (x, v), textcoords="offset points", xytext=(0, 9), ha="center", color=INK2, fontsize=9)
ax.set_xticks(xs)
ax.set_xticklabels([f"{b} bits\n{2**(b-6)}x{2048//2**(b-6)}" for b in xs])
ax.set_xlabel("History / hash length  (sets x ways, always 2048 entries, 6-bit tag)")
ax.set_ylabel("Average MPKI")
ax.yaxis.grid(True, color=GRID, lw=0.8); ax.set_axisbelow(True)
ax.set_ylim(min(vs) - 0.5, max(vs) + 0.6)
ax.set_title("History length at equal storage", loc="left", color=INK, fontsize=12, fontweight="bold")
fig.tight_layout(); fig.savefig("figures/history_length.png", dpi=150)

# ---- Figure 4: loop predictor variants -----------------------------------
lp_labels = {
 "pm_loop": "Submitted: loop first, 3 repeats, 128 entries",
 "lp_loopfirst_conf0": "Loop first, use immediately (0 repeats)",
 "lp_loopfirst_conf1": "Loop first, 1 repeat",
 "lp_loopfirst_conf2": "Loop first, 2 repeats",
 "lp_loopfirst_conf3": "Loop first, 3 repeats (= submitted)",
 "lp_globalfirst_conf0": "Global first (paper order), 0 repeats",
 "lp_globalfirst_conf1": "Global first, 1 repeat",
 "lp_globalfirst_conf2": "Global first, 2 repeats",
 "lp_globalfirst_conf3": "Global first, 3 repeats",
 "lp_64entries": "LPB 64 entries (32 x 2)",
 "lp_256entries": "LPB 256 entries (128 x 2)",
 "lp_512entries": "LPB 512 entries (256 x 2)",
 "lp_128_4way": "LPB 128 entries, 4-way (32 x 4)",
 "lp_index_ip5_0": "LPB index IP[5:0] instead of IP[9:4]",
 "lp_count8bit": "8-bit Count/Limit (loops up to 255)",
 "lp_galloc_bimodal_wrong": "Global allocates on bimodal (not final) miss",
 "pm_noloop": "No loop predictor",
}
lorder = sorted([p for p in lp_labels if p != "lp_loopfirst_conf3"], key=lambda p: avg[p])
fig, ax = plt.subplots(figsize=(8, 6))
ys = list(range(len(lorder)))[::-1]
ax.barh(ys, [avg[p] for p in lorder], color=[BLUE if p == "pm_loop" else GRAY for p in lorder], height=0.7)
for y, p in zip(ys, lorder):
    ax.text(avg[p] + 0.04, y, f"{avg[p]:.3f}", va="center", color=INK if p == "pm_loop" else INK2,
            fontsize=9, fontweight="bold" if p == "pm_loop" else "normal", zorder=4,
            bbox=dict(facecolor=SURF, edgecolor="none", pad=1.0))
ax.axvline(avg["pm_noloop"], color=INK2, lw=1, ls=(0, (4, 3)), zorder=1)
ax.set_yticks(ys); ax.set_yticklabels([lp_labels[p] for p in lorder])
ax.set_xlabel("Average MPKI over 20 traces - lower is better\n(dashed line = no loop predictor, 5.849)")
ax.xaxis.grid(True, color=GRID, lw=0.8); ax.set_axisbelow(True)
ax.set_xlim(5.0, max(avg[p] for p in lorder) + 0.45)
ax.set_title("Loop predictor variants", loc="left", color=INK, fontsize=12, fontweight="bold")
fig.tight_layout(); fig.savefig("figures/loop_variants.png", dpi=150)

# ---- Figure 5: per-trace change from adding the loop predictor -----------
chg = [(t, (mpki[(t, "pm_loop")] - mpki[(t, "pm_noloop")]) / mpki[(t, "pm_noloop")] * 100) for t in traces]
fig, ax = plt.subplots(figsize=(8, 6))
ys = list(range(len(chg)))[::-1]
ax.barh(ys, [c for _, c in chg], color=[BLUE if c < 0 else "#e34948" for _, c in chg], height=0.7)
for y, (t, c) in zip(ys, chg):
    ax.text(c + (-0.5 if c < 0 else 0.5), y, f"{c:+.1f}%", va="center", ha="right" if c < 0 else "left", color=INK2, fontsize=8.5)
ax.axvline(0, color=INK2, lw=0.8)
ax.set_yticks(ys); ax.set_yticklabels([t for t, _ in chg])
ax.set_xlim(min(c for _, c in chg) - 5, max(c for _, c in chg) + 5)
ax.set_xlabel("Change in MPKI from adding the loop predictor (negative = fewer mispredictions)")
ax.xaxis.grid(True, color=GRID, lw=0.8); ax.set_axisbelow(True)
ax.set_title("Effect of the loop predictor per trace", loc="left", color=INK, fontsize=12, fontweight="bold")
fig.tight_layout(); fig.savefig("figures/loop_effect_per_trace.png", dpi=150)

# ---- Markdown tables for the report ---------------------------------------
st = {r["trace"]: r for r in rows if r["predictor"] == "pm_loop"}
sn = {r["trace"]: r for r in rows if r["predictor"] == "pm_noloop"}
with open("results/summary_tables.md", "w") as f:
    f.write("Per-trace MPKI\n\n| Trace | Bimodal only | gshare 32K | Bimodal + global | **+ loop (submitted)** | Change from loop |\n")
    f.write("|---|---:|---:|---:|---:|---:|\n")
    for t in traces:
        a, b = mpki[(t,'pm_noloop')], mpki[(t,'pm_loop')]
        f.write(f"| {t} | {mpki[(t,'bimodal_4K')]:.3f} | {mpki[(t,'gshare_32K_h15')]:.3f} | {a:.3f} | **{b:.3f}** | {(b-a)/a*100:+.1f}% |\n")
    a, b = avg['pm_noloop'], avg['pm_loop']
    f.write(f"| **Average** | {avg['bimodal_4K']:.3f} | {avg['gshare_32K_h15']:.3f} | {a:.3f} | **{b:.3f}** | {(b-a)/a*100:+.1f}% |\n\n")

    f.write("Loop predictor usage (submitted predictor)\n\n| Trace | Branches predicted by the LPB | LPB accuracy |\n|---|---:|---:|\n")
    for t in traces:
        r = st[t]
        f.write(f"| {t} | {float(r['loop_used_rate'])*100:.1f}% | {float(r['loop_used_accuracy'])*100:.1f}% |\n")
    lu = sum(float(st[t]['loop_used_rate']) for t in traces) / len(traces)
    la = sum(float(st[t]['loop_used_accuracy']) for t in traces) / len(traces)
    f.write(f"| **Average** | {lu*100:.1f}% | {la*100:.1f}% |\n\n")
    f.write("Bimodal + global only (no loop predictor): component statistics\n\n| Trace | Global hit rate | Global accuracy on hits | Bimodal accuracy when used |\n|---|---:|---:|---:|\n")
    for t in traces:
        r = sn[t]
        f.write(f"| {t} | {float(r['global_hit_rate'])*100:.1f}% | {float(r['global_hit_accuracy'])*100:.1f}% | {float(r['bimodal_used_accuracy'])*100:.1f}% |\n")
    f.write("\n")

    allab = dict(labels); allab.update(lp_labels)
    f.write("All configurations\n\n| Configuration | Avg MPKI | vs. submitted |\n|---|---:|---:|\n")
    for p in sorted(preds, key=lambda p: avg[p]):
        f.write(f"| {allab.get(p, 'Always predict taken')} (`{p}`) | {avg[p]:.3f} | {avg[p]-avg['pm_loop']:+.3f} |\n")
print("figures and results/summary_tables.md written")
