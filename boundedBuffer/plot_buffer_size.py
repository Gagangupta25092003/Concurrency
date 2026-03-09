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
NUM_PRODUCERS = 2
NUM_CONSUMERS = 8

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


def run_benchmark(bin_path, buf_size):
    result = subprocess.run(
        [bin_path, str(NUM_PRODUCERS), str(NUM_CONSUMERS), str(buf_size)],
        cwd=SCRIPT_DIR, capture_output=True, text=True, timeout=600,
    )
    if result.returncode != 0:
        raise RuntimeError(f"exited {result.returncode}\n{result.stderr}")
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
    print("Building binaries...", flush=True)
    build_binaries()
    os.makedirs(GRAPHS_DIR, exist_ok=True)

    configs = [
        ("Mutex+CV", os.path.join(SCRIPT_DIR, "main_cv")),
        ("Semaphore", os.path.join(SCRIPT_DIR, "main_sem")),
    ]

    metric_labels = ["Wall time (s)", "CPU time (s)", "Time per task (s)", "CPU / Wall ratio"]
    style = {
        ("Mutex+CV", "light"):   {"color": "#3498db", "ls": "-",  "marker": "o"},
        ("Mutex+CV", "heavy"):   {"color": "#2980b9", "ls": "--", "marker": "s"},
        ("Semaphore", "light"):  {"color": "#e74c3c", "ls": "-",  "marker": "^"},
        ("Semaphore", "heavy"):  {"color": "#c0392b", "ls": "--", "marker": "D"},
    }

    fig, axes = plt.subplots(2, 2, figsize=(12, 9))
    axes = axes.flatten()

    for impl_name, bin_path in configs:
        light_data = {i: [] for i in range(4)}
        heavy_data = {i: [] for i in range(4)}
        print(f"Running {impl_name} across buffer sizes: {BUFFER_SIZES}")
        for bsz in BUFFER_SIZES:
            print(f"  buffer_size={bsz} ...", end=" ", flush=True)
            out = run_benchmark(bin_path, bsz)
            pt = parse_per_task(out)
            for i in range(4):
                light_data[i].append(avg_category(pt, LIGHT, i))
                heavy_data[i].append(avg_category(pt, HEAVY, i))
            print("done")

        for ax, i, ylabel in zip(axes, range(4), metric_labels):
            s_l = style[(impl_name, "light")]
            s_h = style[(impl_name, "heavy")]
            ax.plot(BUFFER_SIZES, light_data[i], marker=s_l["marker"], linestyle=s_l["ls"],
                    linewidth=2, markersize=7, color=s_l["color"],
                    label=f"{impl_name} light")
            ax.plot(BUFFER_SIZES, heavy_data[i], marker=s_h["marker"], linestyle=s_h["ls"],
                    linewidth=2, markersize=7, color=s_h["color"],
                    label=f"{impl_name} heavy")

    for ax, ylabel in zip(axes, metric_labels):
        ax.set_xlabel("Buffer capacity")
        ax.set_ylabel(ylabel)
        ax.set_title(ylabel)
        ax.legend(fontsize=7)
        ax.grid(True, alpha=0.3)

    plt.suptitle(f"Buffer Size Effect — Light vs Heavy ({NUM_PRODUCERS} producers, "
                 f"{NUM_CONSUMERS} consumers)", fontsize=13, y=1.02)
    plt.tight_layout()
    out_path = os.path.join(GRAPHS_DIR, "buffer_size_effect.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Saved plot to {out_path}")


if __name__ == "__main__":
    main()
