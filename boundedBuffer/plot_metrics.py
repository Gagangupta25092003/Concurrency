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
MAIN_BIN = os.path.join(SCRIPT_DIR, "main_cv")
THREAD_COUNTS = [1, 2, 4, 8, 16, 32, 64]

RE_MONOTONIC = re.compile(r"time_takenMonotonic.*?:\s+([\d.]+)")
RE_PROCESS = re.compile(r"time_takenProcess.*?:\s+([\d.]+)")
RE_PER_TASK = re.compile(r"time_takenPerTask.*?:\s+([\d.]+)")
RE_CPU_VS_WALL = re.compile(r"cpuTimeVsWallTime.*?:\s+([\d.]+)")


def build():
    cmd = [
        "gcc", "-o", "main_cv",
        "main.c", "boundedBuffer_mutex_cv.c", "producer_consumer.c", "tasks.c", "task.c",
        "-lpthread"
    ]
    result = subprocess.run(cmd, cwd=SCRIPT_DIR, capture_output=True, text=True, timeout=60)
    if result.returncode != 0:
        raise RuntimeError(f"Build failed: {result.stderr}")


def run_benchmark(num_threads):
    result = subprocess.run(
        [MAIN_BIN, str(num_threads)],
        cwd=SCRIPT_DIR, capture_output=True, text=True, timeout=600,
    )
    if result.returncode != 0:
        raise RuntimeError(f"main_cv exited {result.returncode}\n{result.stderr}")
    return result.stdout + result.stderr


def parse_metrics(output):
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
    print("Building main_cv (Mutex+CV)...", flush=True)
    build()
    os.makedirs(GRAPHS_DIR, exist_ok=True)

    data = {
        "time_takenMonotonic": [],
        "time_takenProcess": [],
        "time_takenPerTask": [],
        "cpuTimeVsWallTime": [],
    }

    print("Running benchmarks for thread counts:", THREAD_COUNTS)
    for n in THREAD_COUNTS:
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
        ("time_takenMonotonic", "Wall time (s)", "Wall Time (Monotonic)"),
        ("time_takenProcess", "CPU time (s)", "Process CPU Time"),
        ("time_takenPerTask", "Time per task (s)", "Time Per Task"),
        ("cpuTimeVsWallTime", "Ratio", "CPU / Wall Time Ratio"),
    ]

    for ax, (key, ylabel, title) in zip(axes, titles):
        ax.plot(THREAD_COUNTS, data[key], marker="o", linewidth=2, markersize=8, color="#3498db")
        ax.set_xlabel("Consumer threads")
        ax.set_ylabel(ylabel)
        ax.set_title(title)
        ax.set_xticks(THREAD_COUNTS)
        ax.grid(True, alpha=0.3)

    plt.suptitle("Bounded Buffer — Mutex+CV", fontsize=13, y=1.02)
    plt.tight_layout()
    out_path = os.path.join(GRAPHS_DIR, "metrics_plot_cv.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Saved plot to {out_path}")


if __name__ == "__main__":
    main()
