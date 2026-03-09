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
THREAD_COUNTS = [4, 8]

RE_MONOTONIC = re.compile(r"time_takenMonotonic.*?:\s+([\d.]+)")
RE_PROCESS = re.compile(r"time_takenProcess.*?:\s+([\d.]+)")
RE_PER_TASK = re.compile(r"time_takenPerTask.*?:\s+([\d.]+)")
RE_CPU_VS_WALL = re.compile(r"cpuTimeVsWallTime.*?:\s+([\d.]+)")


def build_binaries():
    for name, impl in [("main_cv", "boundedBuffer_mutex_cv.c"), ("main_sem", "boundedBuffer_semaphore.c")]:
        cmd = ["gcc", "-o", name, "main.c", impl, "producer_consumer.c", "tasks.c", "task.c", "-lpthread"]
        result = subprocess.run(cmd, cwd=SCRIPT_DIR, capture_output=True, text=True, timeout=60)
        if result.returncode != 0:
            raise RuntimeError(f"Build {name} failed: {result.stderr}")


def run_benchmark(bin_path, num_threads):
    result = subprocess.run(
        [bin_path, str(num_threads)],
        cwd=SCRIPT_DIR, capture_output=True, text=True, timeout=600,
    )
    if result.returncode != 0:
        raise RuntimeError(f"{bin_path} exited {result.returncode}\n{result.stderr}")
    return result.stdout + result.stderr


def parse_metrics(output):
    monotonic = [float(m) for m in RE_MONOTONIC.findall(output)]
    process = [float(m) for m in RE_PROCESS.findall(output)]
    per_task = [float(m) for m in RE_PER_TASK.findall(output)]
    cpu_vs_wall = [float(m) for m in RE_CPU_VS_WALL.findall(output)]
    n = len(monotonic)
    if n == 0:
        raise ValueError("No metrics found")
    return (
        sum(monotonic) / n,
        sum(process) / n,
        sum(per_task) / n,
        sum(cpu_vs_wall) / n,
    )


def main():
    print("Building main_cv and main_sem...", flush=True)
    build_binaries()
    os.makedirs(GRAPHS_DIR, exist_ok=True)

    configs = [("Mutex+CV", MAIN_CV_BIN), ("Semaphore", MAIN_SEM_BIN)]
    results = {n: {} for n in THREAD_COUNTS}

    for impl_name, bin_path in configs:
        for n in THREAD_COUNTS:
            print(f"  {impl_name} threads={n} ...", end=" ", flush=True)
            out = run_benchmark(bin_path, n)
            results[n][impl_name] = parse_metrics(out)
            print("done")

    x_labels = [
        "Mutex+CV\n4 threads", "Mutex+CV\n8 threads",
        "Semaphore\n4 threads", "Semaphore\n8 threads",
    ]
    metric_keys = ["time_takenMonotonic", "time_takenProcess", "time_takenPerTask", "cpuTimeVsWallTime"]
    metric_titles = [
        "Wall time (s)", "Process CPU time (s)",
        "Time per task (s)", "CPU / Wall ratio",
    ]
    values = {
        "time_takenMonotonic": [
            results[4]["Mutex+CV"][0], results[8]["Mutex+CV"][0],
            results[4]["Semaphore"][0], results[8]["Semaphore"][0],
        ],
        "time_takenProcess": [
            results[4]["Mutex+CV"][1], results[8]["Mutex+CV"][1],
            results[4]["Semaphore"][1], results[8]["Semaphore"][1],
        ],
        "time_takenPerTask": [
            results[4]["Mutex+CV"][2], results[8]["Mutex+CV"][2],
            results[4]["Semaphore"][2], results[8]["Semaphore"][2],
        ],
        "cpuTimeVsWallTime": [
            results[4]["Mutex+CV"][3], results[8]["Mutex+CV"][3],
            results[4]["Semaphore"][3], results[8]["Semaphore"][3],
        ],
    }

    x = np.arange(len(x_labels))
    width = 0.6
    colors = ["#3498db", "#2980b9", "#e74c3c", "#c0392b"]

    fig, axes = plt.subplots(2, 2, figsize=(11, 9))
    axes = axes.flatten()

    for ax, (key, title) in zip(axes, zip(metric_keys, metric_titles)):
        vals = values[key]
        bars = ax.bar(x, vals, width=width, color=colors, edgecolor="black", linewidth=0.5)
        ax.set_ylabel(title)
        ax.set_title(title)
        ax.set_xticks(x)
        ax.set_xticklabels(x_labels)
        ax.grid(True, axis="y", alpha=0.3)
        for b, v in zip(bars, vals):
            label = f"{v:.4f}" if v < 100 else f"{v:.2f}"
            ax.text(b.get_x() + b.get_width() / 2, b.get_height(), label,
                    ha="center", va="bottom", fontsize=8)

    plt.suptitle("Bounded Buffer: Mutex+CV vs Semaphore (4 and 8 threads)", fontsize=12, y=1.02)
    plt.tight_layout()
    out_path = os.path.join(GRAPHS_DIR, "compare_cv_sem.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Saved comparison plot to {out_path}")


if __name__ == "__main__":
    main()
