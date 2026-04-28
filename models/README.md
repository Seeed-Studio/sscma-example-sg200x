# Model Zoo for CV181x TPU

Pre-converted `.cvimodel` files for Sophgo CV181x (SG200X) TPU inference.

## Directory Structure

```
models/
├── detection/        # Object Detection
├── classification/   # Image Classification
├── segmentation/     # Semantic Segmentation
├── pose/             # Pose Estimation
└── face/             # Face Detection & Recognition
```

## Models

### Detection

| Model | File | Size | Input | Precision |
|-------|------|------|-------|-----------|
| YOLO11n | `yolo11n_detection_cv181x_int8.cvimodel` | 3.0 MB | 640x640 | INT8 |
| YOLO11n | `yolo11n_cv181x_int8.cvimodel` | 3.0 MB | 640x640 | INT8 |
| YOLO26n | `yolo26n_cv181x_int8.cvimodel` | 2.9 MB | 640x640 | INT8 |

### Classification

| Model | File | Size | Input | Precision |
|-------|------|------|-------|-----------|
| YOLO11n-cls | `yolo11n_classify_cv181x_int8.cvimodel` | 2.9 MB | 224x224 | INT8 |
| YOLO11n-cls | `yolo11n-cls_cv181x_int8.cvimodel` | 2.9 MB | 224x224 | INT8 |
| YOLO26n-cls | `yolo26n-cls_cv181x_int8.cvimodel` | 2.9 MB | 224x224 | INT8 |

### Segmentation

| Model | File | Size | Input | Precision | ION Memory |
|-------|------|------|-------|-----------|------------|
| YOLO11n-seg | `yolo11n_segment_cv181x_int8.cvimodel` | 3.9 MB | 640x640 | INT8 | — |
| YOLO11n-seg | `yolo11n-seg_cv181x_mix.cvimodel` | 3.8 MB | 640x640 | MIX | — |
| BiSeNetV2 | `bisenetv2_cityscapes_cv181x_int8.cvimodel` | 6.0 MB | 512x1024 | INT8 | 22.16 MB |
| BiSeNetV2 | `bisenetv2_cityscapes_cv181x_int8_qout.cvimodel` | 5.9 MB | 512x1024 | INT8+QO | 18.10 MB |
| PP-LiteSeg | `pp_liteseg_int8.cvimodel` | 9.9 MB | 512x1024 | INT8 | 22.16 MB |
| PP-LiteSeg | `pp_liteseg_int8_qout.cvimodel` | 9.8 MB | 512x1024 | INT8+QO | 26.73 MB |

> **QO** = quant_output: output tensors kept as INT8 to reduce ION memory.

### Pose Estimation

| Model | File | Size | Input | Precision |
|-------|------|------|-------|-----------|
| YOLO11n-pose | `yolo11n_pose_cv181x_int8.cvimodel` | 3.6 MB | 640x640 | INT8 |
| YOLO11n-pose | `yolo11n-pose_cv181x_mix.cvimodel` | 3.5 MB | 640x640 | MIX |
| YOLO11-pose | `yolo11pose_cv181x_bf16.cvimodel` | 7.5 MB | 640x640 | BF16 |

### Face

| Model | File | Size | Input | Precision | ION Memory |
|-------|------|------|-------|-----------|------------|
| SCRFD-500M-KPS | `scrfd_500m_kps_int8.cvimodel` | 766 KB | 640x640 | INT8 | 4.36 MB |
| MobileFaceNet | `mobilefacenet_128d_int8.cvimodel` | 1.1 MB | 112x112 | INT8 | 0.22 MB |

## Usage in Solutions

Each solution references models from this directory or its own `models/` subdirectory:

```bash
# Example: sscma-model
./sscma-model models/detection/yolo11n_detection_cv181x_int8.cvimodel photo.jpg

# Example: face-recognition
./face-recognition identify \
    models/face/scrfd_500m_kps_int8.cvimodel \
    models/face/mobilefacenet_128d_int8.cvimodel \
    photo.jpg facedb.txt
```

## Model Conversion

All models were converted from ONNX to `.cvimodel` using the [onnx-to-cvimodel](https://github.com/Seeed-Studio/ai-skills/tree/main/skills/onnx-to-cvimodel) skill:

```
# Claude Code
/onnx-to-cvimodel model.onnx --platform cv181x --quantize INT8
```

The skill automates the full conversion pipeline (ONNX graph surgery, calibration, TPU-MLIR deployment) and provides model-specific scripts for YOLO, BiSeNetV2, PP-LiteSeg, SCRFD, etc.
