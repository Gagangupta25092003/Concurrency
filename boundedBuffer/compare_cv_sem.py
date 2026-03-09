#!/usr/bin/env python3
import subprocess
import re
import os
import sys

try:
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError:
    print("Install matplotlib and numpy: pip install matplotlib numpy", file=sys.stderr)
    sys.exit(1)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
GRAPHS_DIR = os.path.join(SCRIPT_DIR, "graphs")
MAIN_CV_BIN = os.path.join(SCRIPT_DIR, "main_cv")
MAIN_SEM_BIN = os.path.join(SCRIPT_DIR, "main_sem")
NUM_PRODUCERS = 2
CONSUMER_COUNTS = [4, 8]

RE_MONOTONIC = re.compile(r"time_takenMonotonic.*?:\s+([\d.]+)")
RE_PROCESS = re.compile(r"time_takenProcess.*?:\s+([\d.]+)")
RE_PER_TASK = re.compile(r"time_takenPerTask.*?:\s+([\d.]+)")
RE_CPU_VS_WALL = re.compile(r"cpuTimeVsWallTime.*?:\s+([\d.]+)")

TASK_NAMES = ["Small CPU", "Sleep/Blocking", "Heavy CPU", "File I/O", "Mixed"]
LIGHT = ["Small CPU", "Sleep/Blocking"]
HEAVY = ["Heavy CPU", "File I/O"]


def build_binaries():
    for name, impl in [("main_cv", "boundedBuffer_mutex_cv.c"), ("main_sem", "boundedBuffer_semaphore.c")]:
        cmd = ["gcc", "-o", name, "main.c", impl, "producer_consumer.c", "tasks.c", "task.c", "-lpthread"]
        result = subprocess.run(cmd, cwd=SCRIPT_DIR, capture_output=True, text=True, timeout=60)
        if result.returncode != 0:
            raise RuntimeError(f"Build {name} failed: {result.stderr}")


def run_benchmark(bin_path, num_consumers):
    result = subprocess.run(
        [bin_path, str(NUM_PRODUCERS), str(num_consumers)],
        cwd=SCRIPT_DIR, capture_output=True, text=True, timeout=600,
    )
    if result.returncode != 0:
        raise RuntimeError(f"{bin_path} exited {result.returncode}\n{result.stderr}")
    return result.stdout + result.stderr


def parse_per_task(output):
    mono = [float(m) for m in RE_MONOTONIC.findall(output)]
    proc = [float(m) for m in RE_PROCESS.findall(output)]
    per  = [float(m) for m in RE_PER_TASK.findall(output)]
    ratio = [float(m) for m in RE_CPU_VS_WALL.findall(output)]
    result = {}
    for i, name in enumerate(TASK_NAMES):
        if i < len(mono):
            result[name] = (mono[i], proc[i], per[i], ratio[i])
    return result


def avg_category(per_task, category_names, metric_idx):
    vals = [per_task[n][metric_idx] for n in category_names if n in per_task]
    return sum(vals) / len(vals) if vals else 0.0


def main():
    print("Building main_cv and main_sem...", flush=True)
    build_binaries()
    os.makedirs(GRAPHS_DIR, exist_ok=True)

    configs = [("Mutex+CV", MAIN_CV_BIN), ("Semaphore", MAIN_SEM_BIN)]
    results = {}

    for impl_name, bin_path in configs:
        for n in CONSUMER_COUNTS:
            key = f"{impl_name}_{n}"
            print(f"  {impl_name} consumers={n} ...", end=" ", flush=True)
            out = run_benchmark(bin_path, n)
            results[key] = parse_per_task(out)
            print("done")

    x_labels = []
    light_vals = {i: [] for i in range(4)}
    heavy_vals = {i: [] for i in range(4)}
    for impl_name, _ in configs:
        for n in CONSUMER_COUNTS:
            key = f"{impl_name}_{n}"
            x_labels.append(f"{impl_name}\n{n} cons")
            pt = results[key]
            for i in range(4):
                light_vals[i].append(avg_category(pt, LIGHT, i))
                heavy_vals[i].append(avg_category(pt, HEAVY, i))

    metric_titles = ["Wall time (s)", "CPU time (s)", "Time per task (s)", "CPU / Wall ratio"]
    x = np.arange(len(x_labels))
    width = 0.35

    fig, axes = plt.subplots(2, 2, figsize=(12, 9))
    axes = axes.flatten()

    for ax, i, title in zip(axes, range(4), metric_titles):
        b1 = ax.bar(x - width/2, light_vals[i], width, label="Light tasks", color="#3498db",
                     edgecolor="black", linewidth=0.5)
        b2 = ax.bar(x + width/2, heavy_vals[i], width, label="Heavy tasks", color="#e74c3c",
                     edgecolor="black", linewidth=0.5)
        ax.set_ylabel(title)
        ax.set_title(title)
        ax.set_xticks(x)
        ax.set_xticklabels(x_labels)
        ax.legend(fontsize=8)
        ax.grid(True, axis="y", alpha=0.3)
        for b in [b1, b2]:
            for bar, v in zip(b, [light_vals[i], heavy_vals[i]]):
                pass
        for bar_group, vals in [(b1, light_vals[i]), (b2, heavy_vals[i])]:
            for bar, v in zip(bar_group, vals):
                label = f"{v:.4f}" if v < 100 else f"{v:.1f}"
                ax.text(bar.get_x() + bar.get_width()/2, bar.get_height(), label,
                        ha="center", va="bottom", fontsize=7)

    plt.suptitle(f"Mutex+CV vs Semaphore — Light vs Heavy ({NUM_PRODUCERS} producers)",
                 fontsize=12, y=1.02)
    plt.tight_layout()
    out_path = os.path.join(GRAPHS_DIR, "compare_cv_sem.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Saved comparison plot to {out_path}")


if __name__ == "__main__":
    main()
