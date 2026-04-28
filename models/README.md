# Model Zoo for CV181x TPU

Pre-converted `.cvimodel` files for Sophgo CV181x (SG200X) TPU inference.

## Conversion Tool

All models were converted using the [onnx-to-cvimodel](https://github.com/Seeed-Studio/ai-skills/tree/main/skills/onnx-to-cvimodel) skill:

```
/onnx-to-cvimodel model.onnx --platform cv181x --quantize INT8
```

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

| Model | File | Size | Input | Source | License |
|-------|------|------|-------|--------|---------|
| YOLO11n | `yolo11n_detection_cv181x_int8.cvimodel` | 3.0 MB | 640x640 | [ultralytics/ultralytics](https://github.com/ultralytics/ultralytics) | AGPL-3.0 |
| YOLO26n | `yolo26n_cv181x_int8.cvimodel` | 2.9 MB | 640x640 | [ultralytics/ultralytics](https://github.com/ultralytics/ultralytics) | AGPL-3.0 |

### Classification

| Model | File | Size | Input | Source | License |
|-------|------|------|-------|--------|---------|
| YOLO11n-cls | `yolo11n_classify_cv181x_int8.cvimodel` | 2.9 MB | 224x224 | [ultralytics/ultralytics](https://github.com/ultralytics/ultralytics) | AGPL-3.0 |
| YOLO26n-cls | `yolo26n-cls_cv181x_int8.cvimodel` | 2.9 MB | 224x224 | [ultralytics/ultralytics](https://github.com/ultralytics/ultralytics) | AGPL-3.0 |

### Segmentation

| Model | File | Size | Input | Source | License |
|-------|------|------|-------|--------|---------|
| YOLO11n-seg | `yolo11n_segment_cv181x_int8.cvimodel` | 3.9 MB | 640x640 | [ultralytics/ultralytics](https://github.com/ultralytics/ultralytics) | AGPL-3.0 |
| BiSeNetV2 | `bisenetv2_cityscapes_cv181x_int8_qout.cvimodel` | 5.9 MB | 512x1024 | [CoinCheung/BiSeNet](https://github.com/CoinCheung/BiSeNet) | MIT |
| PP-LiteSeg | `pp_liteseg_int8.cvimodel` | 9.9 MB | 512x1024 | [PaddlePaddle/PaddleSeg](https://github.com/PaddlePaddle/PaddleSeg) | Apache-2.0 |

### Pose Estimation

| Model | File | Size | Input | Source | License |
|-------|------|------|-------|--------|---------|
| YOLO11n-pose | `yolo11n_pose_cv181x_int8.cvimodel` | 3.6 MB | 640x640 | [ultralytics/ultralytics](https://github.com/ultralytics/ultralytics) | AGPL-3.0 |
| YOLO11-pose | `yolo11pose_cv181x_bf16.cvimodel` | 7.5 MB | 640x640 | [ultralytics/ultralytics](https://github.com/ultralytics/ultralytics) | AGPL-3.0 |

### Face

| Model | File | Size | Input | Source | License |
|-------|------|------|-------|--------|---------|
| SCRFD-500M-KPS | `scrfd_500m_kps_int8.cvimodel` | 766 KB | 640x640 | [deepinsight/insightface](https://github.com/deepinsight/insightface) | Non-commercial |
| MobileFaceNet | `mobilefacenet_128d_int8.cvimodel` | 1.1 MB | 112x112 | [foamliu/InsightFace-v3](https://github.com/foamliu/InsightFace-v3) | Apache-2.0 |

## License Notices

- **AGPL-3.0** (YOLO models): Code and pretrained weights require open-sourcing derivative works under AGPL-3.0. For commercial use without open-sourcing, an [Enterprise License](https://www.ultralytics.com/license) from Ultralytics is required.
- **Non-commercial** (SCRFD): The pretrained model weights from InsightFace are for non-commercial research only. For commercial deployment, contact [InsightFace](https://www.insightface.ai).
- **MIT** (BiSeNetV2): Freely permitted for commercial use with attribution.
- **Apache-2.0** (PP-LiteSeg, MobileFaceNet): Freely permitted for commercial use with attribution.

## Usage in Solutions

```bash
# Example: sscma-model
./sscma-model models/detection/yolo11n_detection_cv181x_int8.cvimodel photo.jpg

# Example: face-recognition
./face-recognition identify \
    models/face/scrfd_500m_kps_int8.cvimodel \
    models/face/mobilefacenet_128d_int8.cvimodel \
    photo.jpg facedb.txt
```
