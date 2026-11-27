import csv
import sys
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# Usage: python3 scripts/plot.py results/scaling_1node.csv [results/scaling_nodes.csv]

def load(path):
    rows = defaultdict(dict)
    with open(path) as f:
        for r in csv.DictReader(f):
            n, cores = int(r["max_crossings"]), int(r["cores"])
            t = float(r["total_s"])
            if cores not in rows[n] or t < rows[n][cores]["total_s"]:
                rows[n][cores] = {k: float(v) for k, v in r.items()}
    return rows


def scaling_plots(rows, prefix, xlabel):
    fig1, ax1 = plt.subplots()
    fig2, ax2 = plt.subplots()
    fig3, ax3 = plt.subplots()
    for n in sorted(rows):
        cores = sorted(rows[n])
        times = [rows[n][c]["total_s"] for c in cores]
        base_c = cores[0]
        base_t = rows[n][base_c]["total_s"] * base_c
        speedup = [base_t / t for t in times]
        eff = [s / c for s, c in zip(speedup, cores)]
        ax1.plot(cores, times, "o-", label=f"n <= {n}")
        ax2.plot(cores, speedup, "o-", label=f"n <= {n}")
        ax3.plot(cores, eff, "o-", label=f"n <= {n}")
        print(f"n<={n}")
        for c, t, s, e in zip(cores, times, speedup, eff):
            print(f"  cores={c:5d} time={t:10.2f}s speedup={s:7.2f} efficiency={e:5.2f}")
    top = max(max(r) for r in rows.values())
    ax2.plot([1, top], [1, top], "k--", label="ideal")
    for ax, title, name in [(ax1, "Execution time", "time"), (ax2, "Speedup", "speedup"),
                            (ax3, "Efficiency", "efficiency")]:
        ax.set_xscale("log", base=2)
        ax.set_xlabel(xlabel)
        ax.set_title(title)
        ax.legend()
        ax.grid(True, alpha=0.3)
    ax1.set_yscale("log")
    ax1.set_ylabel("seconds")
    ax2.set_yscale("log", base=2)
    ax2.set_ylabel("T(1) / T(p)")
    ax3.set_ylabel("speedup / cores")
    fig1.savefig(prefix + "_time.png", dpi=150)
    fig2.savefig(prefix + "_speedup.png", dpi=150)
    fig3.savefig(prefix + "_efficiency.png", dpi=150)


def stage_plot(rows, prefix):
    n = max(rows)
    cores = sorted(rows[n])
    stages = ["generate_s", "dedup_s", "refine_s", "certify_s", "nonhyperbolic_s"]
    fig, ax = plt.subplots()
    bottom = [0] * len(cores)
    for s in stages:
        vals = [rows[n][c][s] for c in cores]
        ax.bar([str(c) for c in cores], vals, bottom=bottom, label=s[:-2])
        bottom = [b + v for b, v in zip(bottom, vals)]
    ax.set_xlabel("cores")
    ax.set_ylabel("seconds")
    ax.set_title(f"Time per stage, n <= {n}")
    ax.legend()
    fig.savefig(prefix + "_stages.png", dpi=150)


for path in sys.argv[1:]:
    rows = load(path)
    prefix = path.rsplit(".", 1)[0]
    scaling_plots(rows, prefix, "cores")
    stage_plot(rows, prefix)
    print("wrote", prefix + "_*.png")
