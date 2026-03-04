#!/usr/bin/env python3
"""
Run thread pool benchmark (mutex+CV implementation) for thread counts 1, 2, 4, 8, 16, 32, 64,
parse the 4 metrics, and plot them in 4 separate graphs. Saves to graphs/metrics_plot_cv.png
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
MAIN_CV_BIN = os.path.join(SCRIPT_DIR, "main_cv")
THREAD_COUNTS = [1, 2, 4, 8, 16, 32, 64]

RE_MONOTONIC = re.compile(r"time_takenMonotonic.*?:\s+([\d.]+)")
RE_PROCESS = re.compile(r"time_takenProcess.*?:\s+([\d.]+)")
RE_PER_TASK = re.compile(r"time_takenPerTask.*?:\s+([\d.]+)")
RE_CPU_VS_WALL = re.compile(r"cpuTimeVsWallTime.*?:\s+([\d.]+)")


def build_cv():
    """Build main_cv using threadPool_mutex_cv.c."""
    cmd = [
        "gcc", "-o", "main_cv",
        "main.c", "threadPool_mutex_cv.c", "consumer.c", "tasks.c",
        "-lpthread"
    ]
    result = subprocess.run(cmd, cwd=SCRIPT_DIR, capture_output=True, text=True, timeout=60)
    if result.returncode != 0:
        raise RuntimeError(f"Build failed: {result.stderr}")
    return MAIN_CV_BIN


def run_benchmark(num_threads: int) -> str:
    result = subprocess.run(
        [MAIN_CV_BIN, str(num_threads)],
        cwd=SCRIPT_DIR,
        capture_output=True,
        text=True,
        timeout=600,
    )
    if result.returncode != 0:
        raise RuntimeError(f"main_cv exited {result.returncode}\n{result.stderr}")
    return result.stdout + result.stderr


def parse_metrics(output: str):
    monotonic = [float(m) for m in RE_MONOTONIC.findall(output)]
    process = [float(m) for m in RE_PROCESS.findall(output)]
    per_task = [float(m) for m in RE_PER_TASK.findall(output)]
    cpu_vs_wall = [float(m) for m in RE_CPU_VS_WALL.findall(output)]
    n = len(monotonic)
    if n == 0:
        raise ValueError("No metrics found in output")
    return (
        sum(monotonic) / n,
        sum(process) / n,
        sum(per_task) / n,
        sum(cpu_vs_wall) / n,
    )


def main():
    if not os.path.isfile(MAIN_CV_BIN):
        print("Building main_cv (mutex+CV)...", flush=True)
        build_cv()
    else:
        # Rebuild to be sure
        build_cv()

    os.makedirs(GRAPHS_DIR, exist_ok=True)
    threads = list(THREAD_COUNTS)
    data = {
        "time_takenMonotonic": [],
        "time_takenProcess": [],
        "time_takenPerTask": [],
        "cpuTimeVsWallTime": [],
    }

    print("Running mutex+CV benchmark for thread counts:", threads)
    for n in threads:
        print(f"  threads = {n} ...", end=" ", flush=True)
        out = run_benchmark(n)
        m_mono, m_proc, m_per, m_ratio = parse_metrics(out)
        data["time_takenMonotonic"].append(m_mono)
        data["time_takenProcess"].append(m_proc)
        data["time_takenPerTask"].append(m_per)
        data["cpuTimeVsWallTime"].append(m_ratio)
        print("done")

    fig, axes = plt.subplots(2, 2, figsize=(10, 8))
    axes = axes.flatten()
    titles = [
        ("time_takenMonotonic", "Wall time (s)", "Time Taken (Monotonic / Wall) [Mutex+CV]"),
        ("time_takenProcess", "Process CPU time (s)", "Process CPU Time [Mutex+CV]"),
        ("time_takenPerTask", "Wall time per task (s)", "Time Per Task (Wall) [Mutex+CV]"),
        ("cpuTimeVsWallTime", "Ratio", "CPU Time vs Wall Time (ratio) [Mutex+CV]"),
    ]
    for ax, (key, ylabel, title) in zip(axes, titles):
        ax.plot(threads, data[key], marker="o", linestyle="-", linewidth=2, markersize=8)
        ax.set_xlabel("Number of consumer threads")
        ax.set_ylabel(ylabel)
        ax.set_title(title)
        ax.set_xticks(threads)
        ax.grid(True, alpha=0.3)
    plt.tight_layout()
    out_path = os.path.join(GRAPHS_DIR, "metrics_plot_cv.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Saved plot to {out_path}")


if __name__ == "__main__":
    main()
