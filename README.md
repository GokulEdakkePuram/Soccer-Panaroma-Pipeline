# Soccer Panorama Pipeline — Hiring Test

A C++ pipeline that reads panoramic soccer video and outputs per-frame camera-center tracking as JSON.

## Quick Start

Two ways to build — pick one:

### Option A: Docker (recommended — no host dependencies needed)

Build the dev environment image once (~15 min, compiles OpenCV 4.10), then work inside it with your source mounted:

```bash
docker build --target env -t pipeline-env .
docker run --rm -it -v "$(pwd):/workspace" pipeline-env
```

Everything below (build, run, eval) works unchanged inside this shell. Your edits happen on the host; the container only provides the toolchain.

### Option B: Native build

Requires:

- CMake 3.16+ and a C++17 compiler
- OpenCV **4.7+** (core, imgproc, dnn, videoio) — most distros ship older versions; use Docker if yours does
- FFmpeg (libavformat, libavcodec, libavutil, libswscale)
- nlohmann/json (fetched automatically by CMake)

### Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

./build/pipeline \
  --video videos/Pano_AIC_1789_M1_missed_goal.mp4 \
  --output output/camera_track.json \
  --viz-output output/debug.mp4 \
  --start-frame 0 --end-frame 249
```

#### Example commands

- Full video:

```bash
./build/pipeline \
  --video videos/Pano_AIC_1789_M1_missed_goal.mp4 \
  --output output/camera_track_full.json \
  --viz-output output/debug.mp4
```

(omitting `--start-frame`/`--end-frame` defaults to the whole video, `0` to EOF.)

-  Custom segment — seek mid-video, decode frames 500-599 only
```bash
./build/pipeline \
  --video videos/Pano_AIC_1789_M1_missed_goal.mp4 \
  --output output/camera_track_seg.json \
  --viz-output output/debug.mp4 \
  --start-frame 500 --end-frame 599
```
- Smaller queue depth — stress backpressure with a 1-frame buffer between stages
```bash
./build/pipeline \
  --video videos/Pano_AIC_1789_M1_missed_goal.mp4 \
  --output output/camera_track.json \
  --viz-output output/debug.mp4 \
  --queue-depth 1
```
- Realtime pacing — throttle the reader to the video's actual fps instead of decoding flat-out
```bash
./build/pipeline \
  --video videos/Pano_AIC_1789_M1_missed_goal.mp4 \
  --output output/camera_track.json \
  --realtime
```

## Repository Layout

| Path | Purpose |
|------|---------|
| `include/pipeline/` | Stage interfaces and types |
| `src/pipeline/` | Sequential baseline pipeline, YOLOv8 detector, metrics |
| `src/stubs/` | Your starting points — `StubVideoReader`, `PassthroughCamera`, `TiledPanoramaDetector` |
| `models/` | YOLOv8n ONNX weights |
| `schemas/` | JSON output contract |
| `eval/` | Automated checks — run them yourself before submitting |
| `videos/` | Sample panoramic soccer video |
|`output/`| Contains evaluation results (`eval/`), camera center tracking JSON output conforming to the schema and a debug visualization video showing detections and camera target |

## Design Notes

### Threading

Three stages, two bounded queues, in `Pipeline::run()`:

```
reader thread -> [read_queue] -> detector thread -> [detect_queue] -> main thread (camera + visualizer + metrics)
```

Reader and detector each run on their own thread. Camera, visualizer, and metrics stay on the main thread and run sequentially. Camera smoothing needs the previous frame's target (`prev_target_`), so splitting that stage across threads would make output order-dependent. Detector also stays single-threaded, processing frames strictly in order, for the same reason: it's what keeps two runs of the same segment producing identical `camera_center` values.

Backpressure: `BoundedQueue::push()` blocks when its queue is full, so a slow detector stalls the reader instead of frames piling up in RAM. Peak RSS was approximately 428MB on the long 750-frame run.

Shutdown: end of video closes both queues (`close()`), which drains normally downstream. An exception in any stage goes through a shared `fail()` that records the first error and `abort()`s both queues, unblocking anything sleeping in `push()`/`pop()`. `run()` always joins both threads before returning, success or failure.

`--realtime` paces the reader to the video's fps with a running deadline + `sleep_until`, not `sleep_for` after each frame, otherwise decode time and any stalls accumulate into drift over a long video instead of self-correcting against the schedule.

### Queue depth

`--queue-depth`, default 8, sizes both queues. Tested down to `--queue-depth 1` — still correct order, no deadlock. Doesn't move throughput much here since the detector is ~50x slower (`4.50ms` and `205.86ms`) than the reader. The queues rarely fill regardless of depth.

### Bottleneck

Detector, easily. 750-frame run with the 5-tile detector on:

| Stage | Avg | p95 | Budget @25fps |
|---|---|---|---|
| video_reader | 4.50ms | 5.40ms | OK |
| detector | 205.86ms | 263.25ms | OVER |
| camera | 0.00ms | 0.00ms | OK |
| visualizer | 0.00ms | 0.00ms | OK |

5 tiles means 5 YOLO passes per frame instead of 1. Average throughput is ~4 fps, it is above the eval's 1 FPS floor and is stable between the short and long clip.

Two things that helped:

- `cv::setNumThreads(cores - 4)` in `main.cpp` — OpenCV's DNN threading stops scaling past 6 threads (my system has 10 cores) on this workload; at full core count it was burning CPU on context-switch overhead instead of inference.
- A warmup inference before the timed loop starts in `Pipeline::run()` because the first DNN forward pass allocates buffers and runs several times slower than steady state, which otherwise shows up as a fake outlier in max latency.

The real fix would be running the 5 tile inferences in parallel (they're independent of each other), but that needs multiple detector workers, which the current single-detector-thread design doesn't support without more rework.

### Sample outputs (`output/`)

- `output/camera_track.json`
- `output/debug.mp4` — detection boxes, camera crosshair, crop window


