#!/usr/bin/env python3
"""Export YOLOv8n to ONNX for maintainer use. Run via: uv run scripts/export_yolov8_model.py"""

from pathlib import Path

from ultralytics import YOLO


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    pt_path = root / "models" / "yolov8n.pt"
    onnx_path = root / "models" / "yolov8n.onnx"

    if not pt_path.exists():
        raise SystemExit(
            f"Missing {pt_path}. Download with:\n"
            "  curl -L -o models/yolov8n.pt "
            "https://github.com/ultralytics/assets/releases/download/v8.2.0/yolov8n.pt"
        )

    model = YOLO(str(pt_path))
    model.export(format="onnx", imgsz=640, simplify=True, opset=12, dynamic=False)
    exported = pt_path.with_suffix(".onnx")
    if exported.exists() and exported != onnx_path:
        exported.replace(onnx_path)
    elif Path("yolov8n.onnx").exists() and not onnx_path.exists():
        Path("yolov8n.onnx").replace(onnx_path)
    print(f"Wrote {onnx_path}")


if __name__ == "__main__":
    main()
