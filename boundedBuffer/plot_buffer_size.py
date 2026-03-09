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
BUFFER_SIZES = [1, 5, 10, 25, 50, 100, 250, 500]
NUM_THREADS = 8

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


def run_benchmark(bin_path, num_threads, buf_size):
    result = subprocess.run(
        [bin_path, str(num_threads), str(buf_size)],
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
    print("Building binaries...", flush=True)
    build_binaries()
    os.makedirs(GRAPHS_DIR, exist_ok=True)

    configs = [
        ("Mutex+CV", os.path.join(SCRIPT_DIR, "main_cv")),
        ("Semaphore", os.path.join(SCRIPT_DIR, "main_sem")),
    ]

    fig, axes = plt.subplots(2, 2, figsize=(12, 9))
    axes = axes.flatten()

    metric_names = ["time_takenMonotonic", "time_takenProcess", "time_takenPerTask", "cpuTimeVsWallTime"]
    metric_labels = ["Wall time (s)", "CPU time (s)", "Time per task (s)", "CPU / Wall ratio"]
    colors = {"Mutex+CV": "#3498db", "Semaphore": "#e74c3c"}

    for impl_name, bin_path in configs:
        data = {k: [] for k in metric_names}
        print(f"Running {impl_name} across buffer sizes: {BUFFER_SIZES}")
        for bsz in BUFFER_SIZES:
            print(f"  buffer_size={bsz} ...", end=" ", flush=True)
            out = run_benchmark(bin_path, NUM_THREADS, bsz)
            m = parse_metrics(out)
            data["time_takenMonotonic"].append(m[0])
            data["time_takenProcess"].append(m[1])
            data["time_takenPerTask"].append(m[2])
            data["cpuTimeVsWallTime"].append(m[3])
            print("done")

        for ax, (key, ylabel) in zip(axes, zip(metric_names, metric_labels)):
            ax.plot(BUFFER_SIZES, data[key], marker="o", linewidth=2, markersize=7,
                    label=impl_name, color=colors[impl_name])

    for ax, (key, ylabel) in zip(axes, zip(metric_names, metric_labels)):
        ax.set_xlabel("Buffer capacity")
        ax.set_ylabel(ylabel)
        ax.set_title(ylabel)
        ax.legend()
        ax.grid(True, alpha=0.3)

    plt.suptitle(f"Buffer Size Effect ({NUM_THREADS} consumer threads)", fontsize=13, y=1.02)
    plt.tight_layout()
    out_path = os.path.join(GRAPHS_DIR, "buffer_size_effect.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Saved plot to {out_path}")


if __name__ == "__main__":
    main()
