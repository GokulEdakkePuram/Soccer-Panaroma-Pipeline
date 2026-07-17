#!/usr/bin/env python3
"""Generate per-stage bottleneck report from camera_track.json."""

import argparse
import json
import sys
from pathlib import Path

STAGES = ("video_reader", "detector", "camera", "visualizer")


def percentile(values, p):
    if not values:
        return 0.0
    sorted_vals = sorted(values)
    idx = int(p * (len(sorted_vals) - 1))
    return sorted_vals[idx]


def stage_stats(frames: list) -> dict:
    stats = {}
    for stage in STAGES:
        values = [f["stage_ms"][stage] for f in frames]
        stats[stage] = {
            "p50": percentile(values, 0.50),
            "p95": percentile(values, 0.95),
            "p99": percentile(values, 0.99),
            "avg": sum(values) / len(values) if values else 0.0,
            "max": max(values) if values else 0.0,
        }
    return stats


def find_bottleneck(stats: dict) -> str:
    return max(STAGES, key=lambda s: stats[s]["p95"])


def render_markdown(doc: dict, stats: dict, bottleneck: str) -> str:
    summary = doc["summary"]
    meta = doc["metadata"]
    lines = [
        "# Pipeline Bottleneck Report",
        "",
        f"**Video:** {meta['video']}",
        f"**Segment:** frames {meta['segment']['start_frame']}–{meta['segment']['end_frame']}",
        f"**Frames processed:** {summary['frames_processed']}",
        f"**Average FPS:** {summary['avg_fps']:.2f}",
        f"**Bottleneck stage (by p95):** `{bottleneck}`",
        "",
        "## Per-Stage Latency (ms)",
        "",
        "| Stage | Avg | p50 | p95 | p99 | Max | Budget @25fps |",
        "|-------|-----|-----|-----|-----|-----|---------------|",
    ]

    budget = 1000.0 / meta.get("fps", 25.0)
    for stage in STAGES:
        s = stats[stage]
        status = "OK" if s["p95"] < budget else "OVER"
        lines.append(
            f"| {stage} | {s['avg']:.2f} | {s['p50']:.2f} | {s['p95']:.2f} | "
            f"{s['p99']:.2f} | {s['max']:.2f} | {budget:.1f}ms ({status}) |"
        )

    lines.extend([
        "",
        "## Discussion Points",
        "",
        f"- The `{bottleneck}` stage dominates p95 latency.",
        "- Ask the candidate: how would they parallelize or optimize the bottleneck?",
        "- Check whether queue depth is appropriate for the slowest stage.",
        "",
    ])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate bottleneck report")
    parser.add_argument("json_path", type=Path)
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args()

    with args.json_path.open() as f:
        doc = json.load(f)

    stats = stage_stats(doc["frames"])
    bottleneck = find_bottleneck(stats)
    report = render_markdown(doc, stats, bottleneck)

    if args.output:
        args.output.write_text(report)
        print(f"Wrote {args.output}")
    else:
        print(report)

    return 0


if __name__ == "__main__":
    sys.exit(main())
