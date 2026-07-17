#!/usr/bin/env python3
"""Validate camera_track.json against schema and hard invariants."""

import argparse
import json
import sys
from pathlib import Path

REQUIRED_STAGE_KEYS = ("video_reader", "detector", "camera", "visualizer")


def load_json(path: Path):
    with path.open() as f:
        return json.load(f)


def validate(doc: dict) -> list:
    errors = []

    for key in ("metadata", "frames", "summary"):
        if key not in doc:
            errors.append(f"missing top-level key: {key}")

    if errors:
        return errors

    meta = doc["metadata"]
    for key in ("video", "width", "height", "fps", "segment"):
        if key not in meta:
            errors.append(f"metadata missing key: {key}")

    if "segment" in meta:
        seg = meta["segment"]
        for key in ("start_frame", "end_frame"):
            if key not in seg:
                errors.append(f"metadata.segment missing key: {key}")

    width = meta.get("width", 0)
    height = meta.get("height", 0)
    start = meta.get("segment", {}).get("start_frame", 0)
    end = meta.get("segment", {}).get("end_frame", -1)

    frames = doc["frames"]
    if not isinstance(frames, list):
        errors.append("frames must be an array")
        return errors

    expected_count = None
    if end >= start:
        expected_count = end - start + 1

    prev_idx = None
    for i, frame in enumerate(frames):
        prefix = f"frames[{i}]"
        for key in ("frame_idx", "timestamp_ms", "camera_center", "stage_ms"):
            if key not in frame:
                errors.append(f"{prefix} missing key: {key}")

        if "camera_center" in frame:
            cc = frame["camera_center"]
            for axis in ("x", "y"):
                if axis not in cc:
                    errors.append(f"{prefix}.camera_center missing {axis}")
                else:
                    val = cc[axis]
                    limit = width if axis == "x" else height
                    if val < 0 or val > limit:
                        errors.append(f"{prefix}.camera_center.{axis}={val} out of bounds [0, {limit}]")

        if "stage_ms" in frame:
            for stage in REQUIRED_STAGE_KEYS:
                if stage not in frame["stage_ms"]:
                    errors.append(f"{prefix}.stage_ms missing {stage}")
                elif frame["stage_ms"][stage] < 0:
                    errors.append(f"{prefix}.stage_ms.{stage} must be >= 0")

        idx = frame.get("frame_idx")
        if prev_idx is not None and idx is not None:
            if idx != prev_idx + 1:
                errors.append(f"{prefix} frame_idx={idx} not consecutive after {prev_idx}")
        prev_idx = idx

    summary = doc["summary"]
    for key in ("frames_processed", "avg_fps", "stage_ms_avg"):
        if key not in summary:
            errors.append(f"summary missing key: {key}")

    if expected_count is not None and summary.get("frames_processed") != expected_count:
        errors.append(
            f"summary.frames_processed={summary.get('frames_processed')} "
            f"!= expected {expected_count}"
        )

    if len(frames) != summary.get("frames_processed"):
        errors.append(
            f"frames array length {len(frames)} != summary.frames_processed "
            f"{summary.get('frames_processed')}"
        )

    if "stage_ms_avg" in summary:
        for stage in REQUIRED_STAGE_KEYS:
            if stage not in summary["stage_ms_avg"]:
                errors.append(f"summary.stage_ms_avg missing {stage}")

    return errors


def main():
    parser = argparse.ArgumentParser(description="Validate pipeline JSON output")
    parser.add_argument("json_path", type=Path)
    args = parser.parse_args()

    doc = load_json(args.json_path)
    errors = validate(doc)

    if errors:
        print("FAIL: validation errors:")
        for err in errors:
            print(f"  - {err}")
        return 1

    print(f"PASS: {args.json_path} is valid ({doc['summary']['frames_processed']} frames)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
