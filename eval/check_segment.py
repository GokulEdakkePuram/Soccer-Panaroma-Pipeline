#!/usr/bin/env python3
"""Verify arbitrary segment processing produces correct frame indices."""

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
        print(f"Pipeline failed for segment [{start}, {end}]: {result.stderr}", file=sys.stderr)
        sys.exit(2)
    with output.open() as f:
        return json.load(f)


def check_segment(doc: dict, start: int, end: int) -> list:
    errors = []
    expected = end - start + 1
    frames = doc["frames"]

    if doc["summary"]["frames_processed"] != expected:
        errors.append(f"expected {expected} frames, got {doc['summary']['frames_processed']}")

    if len(frames) != expected:
        errors.append(f"frames array length {len(frames)} != expected {expected}")

    for i, frame in enumerate(frames):
        expected_idx = start + i
        if frame["frame_idx"] != expected_idx:
            errors.append(f"frames[{i}].frame_idx={frame['frame_idx']} != expected {expected_idx}")

    if frames and frames[0]["frame_idx"] != start:
        errors.append(f"first frame_idx={frames[0]['frame_idx']} != start {start}")
    if frames and frames[-1]["frame_idx"] != end:
        errors.append(f"last frame_idx={frames[-1]['frame_idx']} != end {end}")

    return errors


def main():
    parser = argparse.ArgumentParser(description="Check segment correctness")
    parser.add_argument("--pipeline", type=Path, required=True)
    parser.add_argument("--video", type=Path, required=True)
    parser.add_argument("--thresholds", type=Path, default=Path(__file__).parent / "thresholds.json")
    args = parser.parse_args()

    with args.thresholds.open() as f:
        thresholds = json.load(f)

    all_errors = []
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        for i, seg in enumerate(thresholds["segment_checks"]):
            start = seg["start_frame"]
            end = seg["end_frame"]
            out = tmp_path / f"segment_{i}.json"
            doc = run_segment(args.pipeline, args.video, out, start, end)
            errors = check_segment(doc, start, end)
            if errors:
                all_errors.append(f"segment [{start}, {end}]:")
                all_errors.extend(f"  - {e}" for e in errors)
            else:
                print(f"PASS: segment [{start}, {end}] ({end - start + 1} frames)")

    if all_errors:
        print("FAIL:")
        for line in all_errors:
            print(line)
        return 1

    print("PASS: all segments correct")
    return 0


if __name__ == "__main__":
    sys.exit(main())
