# Soccer Panorama Pipeline — Hiring Test

Build a C++ pipeline that reads panoramic soccer video and outputs per-frame camera-center tracking as JSON.

**Read [TASK.md](TASK.md) for the assignment: three tasks (video reader, concurrent pipeline, camera stage) plus an optional bonus.**

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

The skeleton builds and runs out of the box, but prints `StubVideoReader: no frames will be produced.` and writes an empty track — implementing the video reader is Task 1.

The sample video (`videos/`) and YOLOv8n model (`models/`) are included in this distribution.

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

## What We Evaluate

1. **Video reader** — streaming decode, correct seeking and segment handling
2. **Pipeline** — concurrent stages, bounded queues, stable throughput and flat memory
3. **Camera stage** — detections filtered and smoothed into a follow target

Run the same automated checks we use before submitting:

```bash
./eval/run_eval.sh
```

Works out of the box in the Docker dev container. For native runs it uses [uv](https://docs.astral.sh/uv/) if installed (`uv sync --group eval`), otherwise plain `python3` with `tqdm` and `opencv-python-headless` installed. Tracking accuracy is **not** graded.

## Submission

Include in your README:

- Build instructions and example commands (short clip + full video)
- Design notes: threading model, queue depths, bottleneck stage
- Path to a sample debug visualization video
