#!/usr/bin/env python3
"""Render a review video overlaying camera target from JSON onto source video."""

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

try:
    import cv2
except ImportError:
    cv2 = None


def load_track(path: Path) -> dict:
    with path.open() as f:
        return json.load(f)


def render_opencv(video: Path, track: dict, output: Path, crop_w: int, crop_h: int):
    frames_by_idx = {f["frame_idx"]: f for f in track["frames"]}
    meta = track["metadata"]
    width = meta["width"]
    height = meta["height"]

    cap = cv2.VideoCapture(str(video))
    if not cap.isOpened():
        raise RuntimeError(f"Cannot open video: {video}")

    fps = cap.get(cv2.CAP_PROP_FPS) or meta.get("fps", 25.0)
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(str(output), fourcc, fps, (width, height))

    start = meta["segment"]["start_frame"]
    end = meta["segment"]["end_frame"]
    frame_idx = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        if frame_idx < start:
            frame_idx += 1
            continue
        if end >= 0 and frame_idx > end:
            break

        entry = frames_by_idx.get(frame_idx)
        if entry:
            cx = int(entry["camera_center"]["x"])
            cy = int(entry["camera_center"]["y"])
            cv2.drawMarker(frame, (cx, cy), (0, 0, 255), cv2.MARKER_CROSS, 24, 2)
            x0 = max(0, cx - crop_w // 2)
            y0 = max(0, cy - crop_h // 2)
            cv2.rectangle(frame, (x0, y0), (x0 + crop_w, y0 + crop_h), (255, 0, 0), 2)
            label = f"frame {frame_idx}"
            cv2.putText(frame, label, (20, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 255, 255), 2)

        writer.write(frame)
        frame_idx += 1

    cap.release()
    writer.release()


def render_ffmpeg_fallback(video: Path, track: dict, output: Path):
    """Minimal fallback when OpenCV is not installed."""
    print("OpenCV not available; copying source segment as review placeholder", file=sys.stderr)
    subprocess.run(
        ["ffmpeg", "-y", "-i", str(video), "-c", "copy", str(output)],
        check=True,
        capture_output=True,
    )


def main():
    parser = argparse.ArgumentParser(description="Render visual review video")
    parser.add_argument("--video", type=Path, required=True)
    parser.add_argument("--track", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--crop-width", type=int, default=1920)
    parser.add_argument("--crop-height", type=int, default=1080)
    args = parser.parse_args()

    track = load_track(args.track)
    args.output.parent.mkdir(parents=True, exist_ok=True)

    if cv2 is not None:
        render_opencv(args.video, track, args.output, args.crop_width, args.crop_height)
    else:
        render_ffmpeg_fallback(args.video, track, args.output)

    print(f"Wrote review video: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
