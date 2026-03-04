#!/usr/bin/env python3
"""
Compare metrics for 4 and 8 consumer threads: Mutex vs Mutex+CV.
Uses Small task metrics only (main.c run in small-tasks-only mode with producer delay).
Builds main_mutex and main_cv, runs each with 4 and 8 threads, plots comparison.
"""

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
MAIN_MUTEX_BIN = os.path.join(SCRIPT_DIR, "main_mutex")
MAIN_CV_BIN = os.path.join(SCRIPT_DIR, "main_cv")
THREAD_COUNTS = [4, 8]

RE_MONOTONIC = re.compile(r"time_takenMonotonic.*?:\s+([\d.]+)")
RE_PROCESS = re.compile(r"time_takenProcess.*?:\s+([\d.]+)")
RE_PER_TASK = re.compile(r"time_takenPerTask.*?:\s+([\d.]+)")
RE_CPU_VS_WALL = re.compile(r"cpuTimeVsWallTime.*?:\s+([\d.]+)")


def build_binaries():
    """Build main_mutex (threadPool.c) and main_cv (threadPool_mutex_cv.c)."""
    for name, impl in [("main_mutex", "threadPool_mutex.c"), ("main_cv", "threadPool_mutex_cv.c")]:
        cmd = ["gcc", "-o", name, "main.c", impl, "consumer.c", "tasks.c", "-lpthread"]
        result = subprocess.run(cmd, cwd=SCRIPT_DIR, capture_output=True, text=True, timeout=60)
        if result.returncode != 0:
            raise RuntimeError(f"Build {name} failed: {result.stderr}")
    return MAIN_MUTEX_BIN, MAIN_CV_BIN


def run_benchmark(bin_path: str, num_threads: int) -> str:
    result = subprocess.run(
        [bin_path, str(num_threads)],
        cwd=SCRIPT_DIR,
        capture_output=True,
        text=True,
        timeout=900,  # 15 min per run (1000 * 100ms producer delay + work)
    )
    if result.returncode != 0:
        raise RuntimeError(f"{bin_path} exited {result.returncode}\n{result.stderr}")
    return result.stdout + result.stderr


def parse_metrics(output: str):
    """Extract the 4 metrics. With main running only small tasks, there is 1 block (index 0)."""
    monotonic = RE_MONOTONIC.findall(output)
    process = RE_PROCESS.findall(output)
    per_task = RE_PER_TASK.findall(output)
    cpu_vs_wall = RE_CPU_VS_WALL.findall(output)
    if len(monotonic) < 1:
        raise ValueError("No task blocks in output")
    idx = 0 if len(monotonic) == 1 else 4  # 0 = single block (Small), 4 = Mixed in 5-block run
    return (
        float(monotonic[idx]),
        float(process[idx]),
        float(per_task[idx]),
        float(cpu_vs_wall[idx]),
    )


def main():
    print("Building main_mutex and main_cv...", flush=True)
    build_binaries()

    os.makedirs(GRAPHS_DIR, exist_ok=True)

    # [ (impl_name, bin_path), ... ]
    configs = [("Mutex", MAIN_MUTEX_BIN), ("Mutex+CV", MAIN_CV_BIN)]
    # results[thread_count][impl_name] = (mono, process, per_task, ratio)
    results = {n: {} for n in THREAD_COUNTS}

    for impl_name, bin_path in configs:
        for n in THREAD_COUNTS:
            print(f"  {impl_name} threads={n} (Small tasks) ...", end=" ", flush=True)
            out = run_benchmark(bin_path, n)
            results[n][impl_name] = parse_metrics(out)
            print("done")

    # Build data for grouped bar charts: for each metric, 4 bars = Mutex 4, Mutex 8, Mutex+CV 4, Mutex+CV 8
    x_labels = ["Mutex\n4 threads", "Mutex\n8 threads", "Mutex+CV\n4 threads", "Mutex+CV\n8 threads"]
    metric_keys = ["time_takenMonotonic", "time_takenProcess", "time_takenPerTask", "cpuTimeVsWallTime"]
    metric_titles = [
        "Wall time (s) [Small]",
        "Process CPU time (s) [Small]",
        "Wall time per task (s) [Small]",
        "CPU vs Wall ratio [Small]",
    ]
    values = {
        "time_takenMonotonic": [
            results[4]["Mutex"][0], results[8]["Mutex"][0],
            results[4]["Mutex+CV"][0], results[8]["Mutex+CV"][0],
        ],
        "time_takenProcess": [
            results[4]["Mutex"][1], results[8]["Mutex"][1],
            results[4]["Mutex+CV"][1], results[8]["Mutex+CV"][1],
        ],
        "time_takenPerTask": [
            results[4]["Mutex"][2], results[8]["Mutex"][2],
            results[4]["Mutex+CV"][2], results[8]["Mutex+CV"][2],
        ],
        "cpuTimeVsWallTime": [
            results[4]["Mutex"][3], results[8]["Mutex"][3],
            results[4]["Mutex+CV"][3], results[8]["Mutex+CV"][3],
        ],
    }

    x = np.arange(len(x_labels))
    width = 0.6

    fig, axes = plt.subplots(2, 2, figsize=(11, 9))
    axes = axes.flatten()
    colors_mutex = ["#2ecc71", "#27ae60"]
    colors_cv = ["#3498db", "#2980b9"]
    bar_colors = [colors_mutex[0], colors_mutex[1], colors_cv[0], colors_cv[1]]

    for ax, (key, title) in zip(axes, zip(metric_keys, metric_titles)):
        vals = values[key]
        bars = ax.bar(x, vals, width=width, color=bar_colors, edgecolor="black", linewidth=0.5)
        ax.set_ylabel(title)
        ax.set_title(title)
        ax.set_xticks(x)
        ax.set_xticklabels(x_labels)
        ax.grid(True, axis="y", alpha=0.3)
        for b, v in zip(bars, vals):
            ax.text(b.get_x() + b.get_width() / 2, b.get_height(), f"{v:.4f}" if v < 100 else f"{v:.2f}",
                    ha="center", va="bottom", fontsize=8, rotation=0)

    plt.suptitle("Small tasks (producer 100ms delay): Mutex vs Mutex+CV (4 and 8 threads)", fontsize=12, y=1.02)
    plt.tight_layout()
    out_path = os.path.join(GRAPHS_DIR, "compare_mutex_cv.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Saved comparison plot to {out_path}")


if __name__ == "__main__":
    main()
