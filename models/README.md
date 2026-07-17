# Models

## yolov8n.onnx

- **Architecture:** Ultralytics YOLOv8n, COCO pretrained
- **Classes used:** person (0), sports ball (32)
- **Size:** ~12.8 MB
- **Source:** Exported from official Ultralytics `yolov8n.pt` (v8.2.0) via `uv run scripts/export_yolov8_model.py`
- **SHA256:** `34b0a4475be6e3e8a2261a4c2e94735120190782d0bc107143d1e8be70a531d0`
- **Note:** ONNX input is fixed at 640×640; `YoloV8Detector` letterboxes tile crops to this size. Requires **OpenCV >= 4.7** for DNN inference.

### Regenerate (maintainers)

```bash
curl -L -o models/yolov8n.pt \
  https://github.com/ultralytics/assets/releases/download/v8.2.0/yolov8n.pt
uv sync --group maintainer
uv run scripts/export_yolov8_model.py
```
