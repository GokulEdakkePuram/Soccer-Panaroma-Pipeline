#!/usr/bin/env python3
"""Run the full evaluation suite with progress reporting."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Sequence

from tqdm import tqdm


@dataclass
class CheckResult:
    name: str
    passed: bool
    output: str = ""


@dataclass
class EvalContext:
    root: Path
    eval_dir: Path
    pipeline: Path
    video: Path
    output_dir: Path
    thresholds_path: Path
    thresholds: dict
    short_frames: int
    long_frames: int
    short_json: Path
    long_json: Path
    review_mp4: Path
    bottleneck_md: Path
    report_path: Path
    time_long_path: Path
    report_lines: list[str] = field(default_factory=list)
    results: list[CheckResult] = field(default_factory=list)


def log(msg: str) -> None:
    tqdm.write(msg)


def load_thresholds(path: Path) -> dict:
    with path.open() as f:
        return json.load(f)


def format_duration(seconds: float) -> str:
    if seconds < 60:
        return f"{seconds:.1f}s"
    minutes = int(seconds // 60)
    secs = seconds - minutes * 60
    return f"{minutes}m {secs:.0f}s"


def p95_stage_ms(doc: dict) -> float:
    p95 = doc.get("summary", {}).get("stage_ms_p95", {})
    return sum(p95.get(k, 0.0) for k in ("video_reader", "detector", "camera", "visualizer"))


class TaskProgress:
    """Single-line tqdm bar while a subprocess runs."""

    def __init__(self, label: str) -> None:
        self.label = label
        self._bar: tqdm | None = None

    def __enter__(self) -> TaskProgress:
        self._bar = tqdm(
            desc=self.label,
            total=None,
            unit="",
            leave=False,
            dynamic_ncols=True,
            bar_format="{desc} | {elapsed}",
        )
        return self

    def __exit__(self, *args: object) -> None:
        if self._bar is not None:
            self._bar.close()

    def refresh(self) -> None:
        if self._bar is not None:
            self._bar.refresh()

    def wait(self, process: subprocess.Popen[str]) -> int:
        assert self._bar is not None
        while True:
            code = process.poll()
            if code is not None:
                return code
            self._bar.refresh()
            time.sleep(0.1)


def run_command(
    cmd: Sequence[str],
    *,
    label: str,
    cwd: Path | None = None,
    stderr_path: Path | None = None,
    show_output: bool = False,
) -> subprocess.CompletedProcess[str]:
    stderr_file = None
    if stderr_path is not None:
        stderr_path.parent.mkdir(parents=True, exist_ok=True)
        stderr_file = stderr_path.open("w")
        stderr_target: int | Path = stderr_file
    else:
        stderr_target = subprocess.PIPE

    try:
        if show_output:
            process = subprocess.Popen(
                list(cmd),
                cwd=str(cwd) if cwd else None,
                stdout=subprocess.PIPE,
                stderr=stderr_target if stderr_file is None else stderr_file,
                text=True,
                bufsize=1,
            )
            assert process.stdout is not None
            stdout_lines: list[str] = []
            with TaskProgress(label) as progress:
                while True:
                    line = process.stdout.readline()
                    if line:
                        stdout_lines.append(line)
                        tqdm.write(line.rstrip())
                    elif process.poll() is not None:
                        break
                    else:
                        progress.refresh()
                        time.sleep(0.1)
            code = process.wait()
            return subprocess.CompletedProcess(list(cmd), code, "".join(stdout_lines), "")

        process = subprocess.Popen(
            list(cmd),
            cwd=str(cwd) if cwd else None,
            stdout=subprocess.PIPE,
            stderr=stderr_target,
            text=True,
        )
        with TaskProgress(label) as progress:
            code = progress.wait(process)
        stdout = process.stdout.read() if process.stdout is not None else ""
        stderr = process.stderr.read() if process.stderr is not None and stderr_file is None else ""
        return subprocess.CompletedProcess(list(cmd), code, stdout or "", stderr or "")
    finally:
        if stderr_file is not None:
            stderr_file.close()


def run_pipeline(
    ctx: EvalContext,
    *,
    label: str,
    output_json: Path,
    start_frame: int,
    end_frame: int,
    capture_time: bool = False,
) -> subprocess.CompletedProcess[str]:
    cmd = [
        str(ctx.pipeline),
        "--video",
        str(ctx.video),
        "--output",
        str(output_json),
        "--start-frame",
        str(start_frame),
        "--end-frame",
        str(end_frame),
    ]

    if capture_time:
        time_cmd = ["/usr/bin/time", "-v", *cmd]
        result = run_command(time_cmd, label=label, stderr_path=ctx.time_long_path)
        if result.returncode != 0:
            log(f"  warning: pipeline exited with code {result.returncode} (continuing eval)")
        return result

    return run_command(cmd, label=label)


def record_check(
    ctx: EvalContext,
    name: str,
    passed: bool,
    output: str,
    *,
    echo_output: bool = True,
) -> None:
    ctx.results.append(CheckResult(name=name, passed=passed, output=output))
    ctx.report_lines.append(f"=== {name} ===")
    if output:
        ctx.report_lines.append(output.rstrip())
    ctx.report_lines.append(f"RESULT: {'PASS' if passed else 'FAIL'}")
    ctx.report_lines.append("")

    log(f"=== {name} ===")
    if echo_output and output:
        for line in output.rstrip().splitlines():
            tqdm.write(line)
    log(f"RESULT: {'PASS' if passed else 'FAIL'}")


def run_python_check(ctx: EvalContext, name: str, cmd: Sequence[str]) -> None:
    result = run_command(list(cmd), label=name, show_output=True)
    output = result.stdout
    if result.stderr:
        output = (output + "\n" + result.stderr).strip()
    passed = result.returncode == 0
    record_check(ctx, name, passed, output, echo_output=False)


def check_performance_from_outputs(ctx: EvalContext) -> None:
    name = "Performance stability"

    with ctx.short_json.open() as f:
        short_doc = json.load(f)
    with ctx.long_json.open() as f:
        long_doc = json.load(f)

    fps_drop_max = ctx.thresholds["fps_drop_ratio_max"]
    p95_growth_max = ctx.thresholds["p95_latency_growth_max"]
    min_fps = ctx.thresholds.get("min_fps", 0.0)

    short_fps = short_doc["summary"]["avg_fps"]
    long_fps = long_doc["summary"]["avg_fps"]
    short_p95 = p95_stage_ms(short_doc)
    long_p95 = p95_stage_ms(long_doc)

    lines = [
        f"Short clip ({ctx.short_frames} frames): avg_fps={short_fps:.2f}, p95_total={short_p95:.2f}ms",
        f"Long clip ({ctx.long_frames} frames): avg_fps={long_fps:.2f}, p95_total={long_p95:.2f}ms",
    ]
    errors: list[str] = []

    if long_fps < min_fps:
        errors.append(
            f"Long clip avg_fps {long_fps:.2f} is below the minimum {min_fps:.1f} FPS"
        )

    if short_fps > 0:
        drop = (short_fps - long_fps) / short_fps
        if drop > fps_drop_max:
            errors.append(
                f"FPS dropped {drop * 100:.1f}% from short ({short_fps:.2f}) to long ({long_fps:.2f}), "
                f"max allowed {fps_drop_max * 100:.0f}%"
            )

    if short_p95 > 0:
        growth = (long_p95 - short_p95) / short_p95
        if growth > p95_growth_max:
            errors.append(
                f"p95 stage latency grew {growth * 100:.1f}% from short ({short_p95:.2f}ms) "
                f"to long ({long_p95:.2f}ms), max allowed {p95_growth_max * 100:.0f}%"
            )

    if errors:
        lines.append("FAIL:")
        lines.extend(f"  - {e}" for e in errors)
        record_check(ctx, name, False, "\n".join(lines))
        return

    lines.append("PASS: performance stability OK")
    record_check(ctx, name, True, "\n".join(lines))


def parse_max_rss(time_file: Path) -> str | None:
    if not time_file.exists():
        return None
    text = time_file.read_text()
    match = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", text)
    if not match:
        return None
    return match.group(0)


def write_report(ctx: EvalContext) -> None:
    header = [
        "# Evaluation Report",
        "",
        f"Pipeline: {ctx.pipeline}",
        f"Video: {ctx.video}",
        f"Date: {datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}",
        "",
    ]
    passed = sum(1 for r in ctx.results if r.passed)
    failed = sum(1 for r in ctx.results if not r.passed)
    footer = [
        "=== Summary ===",
        f"Passed: {passed}",
        f"Failed: {failed}",
    ]

    ctx.report_path.write_text("\n".join(header + ctx.report_lines + footer) + "\n")


def build_context(args: argparse.Namespace) -> EvalContext:
    eval_dir = Path(__file__).resolve().parent
    root = eval_dir.parent
    thresholds_path = args.thresholds
    thresholds = load_thresholds(thresholds_path)
    short_frames = int(thresholds["short_clip_frames"])
    long_frames = int(thresholds["long_clip_frames"])
    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    return EvalContext(
        root=root,
        eval_dir=eval_dir,
        pipeline=args.pipeline,
        video=args.video,
        output_dir=output_dir,
        thresholds_path=thresholds_path,
        thresholds=thresholds,
        short_frames=short_frames,
        long_frames=long_frames,
        short_json=output_dir / "camera_track_short.json",
        long_json=output_dir / "camera_track_long.json",
        review_mp4=output_dir / "review.mp4",
        bottleneck_md=output_dir / "bottleneck_report.md",
        report_path=output_dir / "eval_report.txt",
        time_long_path=output_dir / "time_long.txt",
    )


def parse_args() -> argparse.Namespace:
    eval_dir = Path(__file__).resolve().parent
    root = eval_dir.parent
    parser = argparse.ArgumentParser(description="Run evaluation suite with progress output")
    parser.add_argument(
        "--pipeline",
        type=Path,
        default=Path(os.environ.get("PIPELINE", root / "build" / "pipeline")),
        help="Path to pipeline executable",
    )
    parser.add_argument(
        "--video",
        type=Path,
        default=Path(os.environ.get("VIDEO", root / "videos" / "Pano_AIC_1789_M1_missed_goal.mp4")),
        help="Input video path",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(os.environ.get("OUTPUT_DIR", root / "output" / "eval")),
        help="Directory for eval artifacts",
    )
    parser.add_argument(
        "--thresholds",
        type=Path,
        default=eval_dir / "thresholds.json",
        help="Path to thresholds.json",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    ctx = build_context(args)

    if not ctx.pipeline.is_file() or not os.access(ctx.pipeline, os.X_OK):
        log(f"Pipeline binary not found or not executable: {ctx.pipeline}")
        log("Build first: cmake -S . -B build && cmake --build build")
        return 2

    log("Evaluation configuration:")
    log(f"  pipeline   : {ctx.pipeline}")
    log(f"  video      : {ctx.video}")
    log(f"  output dir : {ctx.output_dir}")
    log(f"  short clip : {ctx.short_frames} frames (~{ctx.short_frames / 25:.0f}s @ 25 fps)")
    log(f"  long clip  : {ctx.long_frames} frames (~{ctx.long_frames / 25:.0f}s @ 25 fps)")

    suite_start = time.monotonic()
    total_steps = 9

    with tqdm(total=total_steps, desc="Evaluation", unit="step", dynamic_ncols=True) as suite_bar:
        def run_step(title: str, fn) -> None:
            suite_bar.set_description(title)
            fn()
            suite_bar.update(1)

        run_step(
            f"Short clip pipeline ({ctx.short_frames} frames)",
            lambda: run_pipeline(
                ctx,
                label="short clip pipeline",
                output_json=ctx.short_json,
                start_frame=0,
                end_frame=ctx.short_frames - 1,
            ),
        )

        run_step(
            f"Long clip pipeline ({ctx.long_frames} frames)",
            lambda: run_pipeline(
                ctx,
                label="long clip pipeline",
                output_json=ctx.long_json,
                start_frame=0,
                end_frame=ctx.long_frames - 1,
                capture_time=True,
            ),
        )

        run_step(
            "Contract validation (short)",
            lambda: run_python_check(
                ctx,
                "Contract validation (short)",
                [sys.executable, str(ctx.eval_dir / "validate_output.py"), str(ctx.short_json)],
            ),
        )

        run_step(
            "Contract validation (long)",
            lambda: run_python_check(
                ctx,
                "Contract validation (long)",
                [sys.executable, str(ctx.eval_dir / "validate_output.py"), str(ctx.long_json)],
            ),
        )

        run_step("Performance stability", lambda: check_performance_from_outputs(ctx))

        run_step(
            "Segment correctness",
            lambda: run_python_check(
                ctx,
                "Segment correctness",
                [
                    sys.executable,
                    str(ctx.eval_dir / "check_segment.py"),
                    "--pipeline",
                    str(ctx.pipeline),
                    "--video",
                    str(ctx.video),
                    "--thresholds",
                    str(ctx.thresholds_path),
                ],
            ),
        )

        run_step(
            "Determinism",
            lambda: run_python_check(
                ctx,
                "Determinism",
                [
                    sys.executable,
                    str(ctx.eval_dir / "check_determinism.py"),
                    "--pipeline",
                    str(ctx.pipeline),
                    "--video",
                    str(ctx.video),
                    "--thresholds",
                    str(ctx.thresholds_path),
                ],
            ),
        )

        run_step("Bottleneck report", lambda: _run_bottleneck_report(ctx))

        run_step("Visual review", lambda: _run_visual_review(ctx))

    rss_line = parse_max_rss(ctx.time_long_path)
    if rss_line:
        log("=== Memory (long clip) ===")
        log(rss_line)
        ctx.report_lines.append("=== Memory (long clip) ===")
        ctx.report_lines.append(rss_line)
        ctx.report_lines.append("")

    write_report(ctx)

    passed = sum(1 for r in ctx.results if r.passed)
    failed = sum(1 for r in ctx.results if not r.passed)
    elapsed = time.monotonic() - suite_start

    log("=== Summary ===")
    log(f"Passed: {passed}")
    log(f"Failed: {failed}")
    log(f"Total time: {format_duration(elapsed)}")
    log(f"Report: {ctx.report_path}")

    if failed > 0:
        log(f"EVALUATION FAILED ({failed} checks)")
        return 1

    log(f"EVALUATION PASSED ({passed} checks)")
    return 0


def _run_bottleneck_report(ctx: EvalContext) -> None:
    result = run_command(
        [
            sys.executable,
            str(ctx.eval_dir / "bottleneck_report.py"),
            str(ctx.long_json),
            "--output",
            str(ctx.bottleneck_md),
        ],
        label="bottleneck report",
        show_output=True,
    )
    bottleneck_output = result.stdout
    if result.stderr:
        bottleneck_output = (bottleneck_output + "\n" + result.stderr).strip()
    ctx.report_lines.append("=== Bottleneck report ===")
    if bottleneck_output:
        ctx.report_lines.append(bottleneck_output.rstrip())
    if ctx.bottleneck_md.exists():
        ctx.report_lines.append(ctx.bottleneck_md.read_text().rstrip())
    ctx.report_lines.append("")


def _run_visual_review(ctx: EvalContext) -> None:
    review_result = run_command(
        [
            sys.executable,
            str(ctx.eval_dir / "render_review.py"),
            "--video",
            str(ctx.video),
            "--track",
            str(ctx.short_json),
            "--output",
            str(ctx.review_mp4),
        ],
        label="visual review",
        show_output=True,
    )
    ctx.report_lines.append("=== Visual review ===")
    review_output = review_result.stdout
    if review_result.stderr:
        review_output = (review_output + "\n" + review_result.stderr).strip()
    if review_output:
        ctx.report_lines.append(review_output.rstrip())
    if review_result.returncode == 0:
        ctx.report_lines.append(f"Review video: {ctx.review_mp4}")
    else:
        ctx.report_lines.append("Warning: visual review generation failed (non-fatal)")
    ctx.report_lines.append("")


if __name__ == "__main__":
    sys.exit(main())
