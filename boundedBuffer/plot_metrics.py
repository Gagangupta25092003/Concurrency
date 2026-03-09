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
NUM_PRODUCERS = 2
THREAD_COUNTS = [1, 2, 4, 8, 16, 32, 64]

RE_MONOTONIC = re.compile(r"time_takenMonotonic.*?:\s+([\d.]+)")
RE_PROCESS = re.compile(r"time_takenProcess.*?:\s+([\d.]+)")
RE_PER_TASK = re.compile(r"time_takenPerTask.*?:\s+([\d.]+)")
RE_CPU_VS_WALL = re.compile(r"cpuTimeVsWallTime.*?:\s+([\d.]+)")

TASK_NAMES = ["Small CPU", "Sleep/Blocking", "Heavy CPU", "File I/O", "Mixed"]
TASK_STYLES = {
    "Small CPU":       {"color": "#3498db", "marker": "o", "ls": "-"},
    "Sleep/Blocking":  {"color": "#85c1e9", "marker": "s", "ls": "--"},
    "Heavy CPU":       {"color": "#e74c3c", "marker": "^", "ls": "-"},
    "File I/O":        {"color": "#f1948a", "marker": "D", "ls": "--"},
    "Mixed":           {"color": "#2ecc71", "marker": "P", "ls": "-."},
}


def build():
    cmd = [
        "gcc", "-o", "main_cv",
        "main.c", "boundedBuffer_mutex_cv.c", "producer_consumer.c", "tasks.c", "task.c",
        "-lpthread"
    ]
    result = subprocess.run(cmd, cwd=SCRIPT_DIR, capture_output=True, text=True, timeout=60)
    if result.returncode != 0:
        raise RuntimeError(f"Build failed: {result.stderr}")


def run_benchmark(num_consumers):
    result = subprocess.run(
        [MAIN_BIN, str(NUM_PRODUCERS), str(num_consumers)],
        cwd=SCRIPT_DIR, capture_output=True, text=True, timeout=600,
    )
    if result.returncode != 0:
        raise RuntimeError(f"main_cv exited {result.returncode}\n{result.stderr}")
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


def main():
    print("Building main_cv (Mutex+CV)...", flush=True)
    build()
    os.makedirs(GRAPHS_DIR, exist_ok=True)

    all_data = {name: {"mono": [], "proc": [], "per": [], "ratio": []} for name in TASK_NAMES}

    print(f"Running benchmarks ({NUM_PRODUCERS} producers) for consumer counts:", THREAD_COUNTS)
    for n in THREAD_COUNTS:
        print(f"  consumers = {n} ...", end=" ", flush=True)
        out = run_benchmark(n)
        per_task = parse_per_task(out)
        for name in TASK_NAMES:
            if name in per_task:
                m = per_task[name]
                all_data[name]["mono"].append(m[0])
                all_data[name]["proc"].append(m[1])
                all_data[name]["per"].append(m[2])
                all_data[name]["ratio"].append(m[3])
        print("done")

    metric_keys = ["mono", "proc", "per", "ratio"]
    metric_labels = ["Wall time (s)", "CPU time (s)", "Time per task (s)", "CPU / Wall ratio"]

    fig, axes = plt.subplots(2, 2, figsize=(12, 9))
    axes = axes.flatten()

    for ax, key, ylabel in zip(axes, metric_keys, metric_labels):
        for name in TASK_NAMES:
            s = TASK_STYLES[name]
            tag = "[light]" if name in ("Small CPU", "Sleep/Blocking") else \
                  "[heavy]" if name in ("Heavy CPU", "File I/O") else "[mixed]"
            ax.plot(THREAD_COUNTS, all_data[name][key],
                    marker=s["marker"], linestyle=s["ls"], linewidth=2, markersize=7,
                    color=s["color"], label=f"{name} {tag}")
        ax.set_xlabel("Consumer threads")
        ax.set_ylabel(ylabel)
        ax.set_title(ylabel)
        ax.set_xticks(THREAD_COUNTS)
        ax.legend(fontsize=7)
        ax.grid(True, alpha=0.3)

    plt.suptitle(f"Bounded Buffer — Mutex+CV ({NUM_PRODUCERS} producers)", fontsize=13, y=1.02)
    plt.tight_layout()
    out_path = os.path.join(GRAPHS_DIR, "metrics_plot_cv.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Saved plot to {out_path}")


if __name__ == "__main__":
    main()
