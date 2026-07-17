#!/usr/bin/env python3
"""Check throughput stability between a short clip and a longer clip."""

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def load_thresholds(path: Path) -> dict:
    with path.open() as f:
        return json.load(f)


def run_pipeline(pipeline_bin: Path, video: Path, output: Path, start: int, end: int) -> dict:
    cmd = [
        str(pipeline_bin),
        "--video", str(video),
        "--output", str(output),
        "--start-frame", str(start),
        "--end-frame", str(end),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Pipeline failed: {result.stderr}", file=sys.stderr)
        sys.exit(2)
    with output.open() as f:
        return json.load(f)


def p95_stage_ms(doc: dict) -> float:
    p95 = doc.get("summary", {}).get("stage_ms_p95", {})
    return sum(p95.get(k, 0.0) for k in ("video_reader", "detector", "camera", "visualizer"))


def main():
    parser = argparse.ArgumentParser(description="Check throughput stability short vs long clip")
    parser.add_argument("--pipeline", type=Path, required=True)
    parser.add_argument("--video", type=Path, required=True)
    parser.add_argument("--thresholds", type=Path, default=Path(__file__).parent / "thresholds.json")
    args = parser.parse_args()

    thresholds = load_thresholds(args.thresholds)
    short_frames = thresholds["short_clip_frames"]
    long_frames = thresholds["long_clip_frames"]
    fps_drop_max = thresholds["fps_drop_ratio_max"]
    p95_growth_max = thresholds["p95_latency_growth_max"]

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        short_out = tmp_path / "short.json"
        long_out = tmp_path / "long.json"

        short_doc = run_pipeline(args.pipeline, args.video, short_out, 0, short_frames - 1)
        long_doc = run_pipeline(args.pipeline, args.video, long_out, 0, long_frames - 1)

    short_fps = short_doc["summary"]["avg_fps"]
    long_fps = long_doc["summary"]["avg_fps"]
    short_p95 = p95_stage_ms(short_doc)
    long_p95 = p95_stage_ms(long_doc)

    errors = []

    if short_fps > 0:
        drop = (short_fps - long_fps) / short_fps
        if drop > fps_drop_max:
            errors.append(
                f"FPS dropped {drop*100:.1f}% from short ({short_fps:.2f}) to long ({long_fps:.2f}), "
                f"max allowed {fps_drop_max*100:.0f}%"
            )

    if short_p95 > 0:
        growth = (long_p95 - short_p95) / short_p95
        if growth > p95_growth_max:
            errors.append(
                f"p95 stage latency grew {growth*100:.1f}% from short ({short_p95:.2f}ms) "
                f"to long ({long_p95:.2f}ms), max allowed {p95_growth_max*100:.0f}%"
            )

    print(f"Short clip ({short_frames} frames): avg_fps={short_fps:.2f}, p95_total={short_p95:.2f}ms")
    print(f"Long clip ({long_frames} frames): avg_fps={long_fps:.2f}, p95_total={long_p95:.2f}ms")

    if errors:
        print("FAIL:")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("PASS: performance stability OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
