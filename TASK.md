# Assignment: Real-Time Soccer Panorama Pipeline

## Problem

Given a panoramic soccer video (3840×800), build a system that automatically follows the game's action. Your pipeline reads the video and writes a JSON file with the **camera center** — the point a virtual camera should look at — for every frame.

This is a **pipeline engineering** exercise. We care about streaming execution, concurrency, clean stage separation, and basic post-processing — not tracking accuracy.

## What is provided

You start from a compiling, runnable skeleton:

- **Sequential baseline pipeline** (`src/pipeline/Pipeline.cpp`) — runs all stages one frame at a time on a single thread, with per-stage timing
- **YOLOv8 detector** (`YoloV8Detector`) — OpenCV DNN inference for persons and the ball; **do not modify**
- **Visualizer** (`OpenCvVisualizer`) — draws detection boxes, camera crosshair, and crop window into a debug video
- **Metrics** (`MetricsCollector`) — writes the output JSON including timing summary
- **Stage interfaces** (`include/pipeline/I*.hpp`) and CLI wiring (`src/main.cpp`)
- Sample video (`videos/`) and ONNX model (`models/`) — included in this distribution

The build works out of the box, but produces no frames until you complete Task 1.

## Your tasks

### Task 1 — Video reader

Implement `IVideoReader` (start from `src/stubs/StubVideoReader.cpp`):

- Stream frames one at a time — never load the whole video into memory
- Support `--start-frame` / `--end-frame` segments, including seeking to the start frame
- Fill `Frame` with `frame_idx`, `timestamp_ms`, dimensions, and RGB pixel data
- Populate `VideoMetadata` (width, height, fps, total frames)

Any backend is fine: raw FFmpeg (libav\* is already linked) or OpenCV `cv::VideoCapture` (already a dependency). We evaluate correctness and streaming behavior, not the backend choice.

### Task 2 — Concurrent pipeline

Re-architect the sequential baseline (`src/pipeline/Pipeline.cpp`) into a streaming, concurrent pipeline:

- Stages run in parallel (e.g. reader / detector / consumer threads)
- **Bounded queues** between stages — `--queue-depth` sets the capacity
- **Backpressure**: a slow stage must stall upstream stages, not let frames pile up
- Clean shutdown at end of stream and on errors
- Keep the `Pipeline` constructor and `run()` contract so `main.cpp` and the eval harness keep working

Constraints your implementation must satisfy:

- **Throughput stability**: FPS and p95 latency must not degrade significantly between a 10 s clip (250 frames) and a 30 s clip (750 frames), and must average **at least 1 FPS**
- **Memory**: usage must not grow with video length
- **Determinism**: the same segment run twice produces identical `camera_center` values

### Task 3 — Camera stage

Implement `ICamera` (start from `src/stubs/PassthroughCamera.cpp`, which hardcodes the frame center):

1. Filter detections by confidence threshold
2. Compute the centroid of player detections (or the largest cluster)
3. Apply exponential smoothing against the previous target
4. Clamp to panorama bounds

A simple approach is sufficient — no production-grade soccer camera policy is expected.

## Bonus (optional): panorama tiling

Running YOLO on the full 3840×800 frame distorts objects. The provided `TiledPanoramaDetector` scaffold (`src/stubs/TiledPanoramaDetector.cpp`) therefore crops the **center tile only** — detections appear only in the middle of the field. Extending it to the full panorama is a bonus:

1. Split the frame into 5 square tiles of 768×768 (helper: `default_tile_layout()`), vertically centered (`y0 = 16`)
2. Run `tile_detector_->process()` on each tile in fixed order (0 → 4), reusing the single `YoloV8Detector` instance
3. Offset each bbox back to panorama coordinates (`x += tile.x0`, `y += tile.y0`)
4. Apply cross-tile NMS (IoU 0.45, same class) — players near tile borders are detected twice
5. Sort deterministically (helpers in `include/pipeline/YoloPostprocess.hpp`)

Pitfalls: forgetting the `y0 = 16` offset, returning tile-local coordinates, reloading the model per tile, non-deterministic ordering after merge. Note five inference passes per frame make the detector the bottleneck — profile before and after.

## CLI

```bash
./build/pipeline \
  --video videos/Pano_AIC_1789_M1_missed_goal.mp4 \
  --output output/camera_track.json \
  --viz-output output/debug.mp4 \
  --start-frame 0 \
  --end-frame 249
```

| Flag | Description |
|------|-------------|
| `--video` | Input video path (required) |
| `--output` | Output JSON path (required) |
| `--viz-output` | Debug visualization video path (optional) |
| `--model` | YOLOv8n ONNX path (default: `models/yolov8n.onnx`) |
| `--start-frame` | First frame to process (default: 0) |
| `--end-frame` | Last frame inclusive, -1 for EOF (default: -1) |
| `--queue-depth` | Bounded queue depth between stages (default: 8) |
| `--realtime` | Pace processing to video frame rate |

## Output JSON

See [`schemas/camera_track.schema.json`](schemas/camera_track.schema.json) and [`examples/expected_output_format.json`](examples/expected_output_format.json). Each frame entry must include:

- `frame_idx`, `timestamp_ms`
- `camera_center`: `{x, y}`
- `stage_ms`: timing for `video_reader`, `detector`, `camera`, `visualizer`

## Deliverables

1. Working pipeline executable (Tasks 1–3 complete)
2. JSON output conforming to the schema
3. A debug visualization video showing detections and camera target
4. README notes: threading model, queue depths, bottleneck stage, example commands

## Evaluation

You can run the automated checks yourself before submitting:

```bash
./eval/run_eval.sh
```

| Priority | Criterion |
|----------|-----------|
| High | Video reader: streaming decode, correct seek and segment handling |
| High | Pipeline: concurrent stages, bounded queues, stable throughput (10 s vs 30 s clips, ≥ 1 FPS) |
| High | Camera stage: filtered, smoothed, clamped follow target (not a hardcoded center) |
| Medium | Per-stage timing in JSON; deterministic output |
| Bonus | Panorama tiling: full 5-tile split/merge with cross-tile NMS |

Not scored: tracking accuracy, camera policy sophistication, choice of video backend.

## Tips

- Do Task 1 first — nothing runs without frames. Verify with `--viz-output` on a short segment
- Profile before optimizing; the detector is usually the bottleneck
- Test with the 10 s clip (`--end-frame 249`) before running the full eval
