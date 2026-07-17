#!/usr/bin/env python3
"""Run the same segment twice and verify camera_center values match."""

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def run_segment(pipeline_bin: Path, video: Path, output: Path, start: int, end: int) -> dict:
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


def compare_runs(run_a: dict, run_b: dict, epsilon: float) -> list:
    errors = []
    frames_a = run_a["frames"]
    frames_b = run_b["frames"]

    if len(frames_a) != len(frames_b):
        errors.append(f"frame count mismatch: {len(frames_a)} vs {len(frames_b)}")
        return errors

    mismatches = 0
    for i, (fa, fb) in enumerate(zip(frames_a, frames_b)):
        if fa["frame_idx"] != fb["frame_idx"]:
            errors.append(f"frame_idx mismatch at index {i}: {fa['frame_idx']} vs {fb['frame_idx']}")
            continue
        for axis in ("x", "y"):
            va = fa["camera_center"][axis]
            vb = fb["camera_center"][axis]
            if abs(va - vb) > epsilon:
                mismatches += 1
                if mismatches <= 5:
                    errors.append(
                        f"frame {fa['frame_idx']} camera_center.{axis}: "
                        f"{va} vs {vb} (diff={abs(va-vb):.6f})"
                    )

    if mismatches > 5:
        errors.append(f"... and {mismatches - 5} more mismatches")

    return errors


def main():
    parser = argparse.ArgumentParser(description="Check determinism")
    parser.add_argument("--pipeline", type=Path, required=True)
    parser.add_argument("--video", type=Path, required=True)
    parser.add_argument("--thresholds", type=Path, default=Path(__file__).parent / "thresholds.json")
    args = parser.parse_args()

    with args.thresholds.open() as f:
        thresholds = json.load(f)

    short_frames = thresholds["short_clip_frames"]
    epsilon = thresholds["determinism_epsilon"]
    end = short_frames - 1

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        out_a = tmp_path / "run_a.json"
        out_b = tmp_path / "run_b.json"
        run_a = run_segment(args.pipeline, args.video, out_a, 0, end)
        run_b = run_segment(args.pipeline, args.video, out_b, 0, end)

    errors = compare_runs(run_a, run_b, epsilon)
    if errors:
        print("FAIL: determinism check failed")
        for e in errors:
            print(f"  - {e}")
        return 1

    print(f"PASS: deterministic output for frames [0, {end}] (epsilon={epsilon})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
