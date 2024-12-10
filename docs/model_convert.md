# Model Convert 

## Introduction

The model conversion tool is designed to convert models from the ONNX format to the cvimodel format for deployment in various applications. This guide provides instructions for converting YOLOv5, YOLOv8, and YOLO11 models for object detection, classification, pose estimation, and segmentation tasks.

## Preparation

There are two ways to install `tpu-mlir`: either by building from source or by using a Docker image. We recommend using the Docker image for simplicity and convenience.

### Environment Prerequisites
- Python >= 3.10
- Ubuntu 22.04

### 1. Install `tpu-mlir` from Source

To install `tpu-mlir` via pip, run the following command:

```bash
pip install tpu_mlir[onnx] #[onnx] [torch] [tensorflow] [caffe] [paddle] [all]
```

**Note**: We highly recommend using a virtual environment (such as `conda`) to install `tpu-mlir` to avoid potential conflicts with other packages.

### 2. Use the Docker Image

For an easier setup, you can use the pre-built Docker image. To pull the latest image, run:

```bash
docker pull sophgo/tpuc_dev:latest
```

This version is more concise and easier to follow, with a focus on clarity and simplicity. Let me know if you'd like further adjustments!


More details please refer to [tpu-mlir](https://github.com/sophgo/tpu-mlir).

### Clone the Repository

```bash
git clone https://github.com/Seeed-Studio/sscma-example-sg200x
cd sscma-example-sg200x
git submodule update --init
```

### Create workspace
```bash
mkdir workspace
```

### 1. Clone the Repository

To get started, clone the repository and initialize the submodules:

```bash
git clone https://github.com/Seeed-Studio/sscma-example-sg200x
cd sscma-example-sg200x
git submodule update --init
```

### 2. Create a Workspace

Create a directory for your workspace:

```bash
mkdir workspace && cd workspace
```

### 3. Prepare the Dataset

To download the dataset from Roboflow, use the following command:

```bash
mkdir dataset && cd dataset
curl -L "https://universe.roboflow.com/ds/y7cg2bEdLV?key=GKdZx5ISuU" > roboflow.zip
unzip roboflow.zip
rm roboflow.zip
```

**Note**: You can use any other dataset as well.

### 4. Prepare the Model

As an example, we will use the YOLO11 model for object detection. To download the model, use the following command:

Export the model from the following link:

[Ultralytics Model Hub](https://hub.ultralytics.com/)

**Note**: Currently, we only support the ONNX format. Please export your model to ONNX format. For other formats, refer to the `tpu-mlir` documentation.

## Conversion

### YOLO11 Object Detection

```bash
python3 export.py model/yolo11n.onnx --output_names /model.23/cv2.0/cv2.0.2/Conv_output_0,/model.23/cv3.0/cv3.0.2/Conv_output_0,/model.23/cv2.1/cv2.1.2/Conv_output_0,/model.23/cv3.1/cv3.1.2/Conv_output_0,/model.23/cv2.2/cv2.2.2/Conv_output_0,/model.23/cv3.2/cv3.2.2/Conv_output_0
```

### YOLO11 Pose Estimation

```bash
python3 export.py model/yolo11n_pose.onnx --quantize_table qtable/yolo11n_pose
```

### YOLO11 Classification
```bash
python3 export.py model/yolo11n_classify.onnx  --input_shapes [[1,3,224,224]]
```

### YOLO11 Segmentation
```bash
python3 export.py model/yolo11n_seg.onnx  --quantize_table qtable/yolo11n_seg
```

## Evaluation

TODO

## Deployment

Please refer to [../README.md](../README.md).

Excute the following command in the device:

```bash
sscma-model xxxx.cvimodel input.jpg output.jpg
```


