# Model Detector

## Overview

This example demonstrates how to combine real-time camera capture with AI model inference using the SSCMA-Micro framework. The program captures video frames from the camera, performs object detection using a pre-trained model, and displays the results with timing statistics.

## Features

- Real-time camera capture with configurable resolution
- AI model inference for object detection
- Configurable confidence threshold for detections
- Performance timing statistics (capture, inference, total processing time)
- Physical address support for zero-copy tensor input
- RTSP streaming capabilities

## Building

1. Navigate to the model_detector directory:
   ```
   cd solutions/model_detector
   ```

2. Create a build directory:
   ```
   mkdir build && cd build
   ```

3. Configure the build with CMake:
   ```
   cmake .. -DCMAKE_TOOLCHAIN_FILE=../../../cmake/toolchain-riscv64-linux-musl-x86_64.cmake
   ```

4. Build the project:
   ```
   make -j$(nproc)
   ```

## Running

Run the compiled binary with optional parameters:

```
./model_detector [model_path] [confidence_threshold]
```

- `model_path`: Path to the AI model file (default: "yolo11n.int8")
- `confidence_threshold`: Minimum confidence for detections (default: 0.5)

Example:
```
./model_detector yolov8  0.7
```

## Output

The program will output timing statistics and detection results for each frame:

```
Frame 1: Capture: 15 ms, Inference: 25 ms, Total: 40 ms
Detections: 2 objects found
- Class 0: 0.85 confidence at [100,200,150,250]
- Class 1: 0.72 confidence at [300,400,350,450]
```

Every 10 frames, it will output the average processing time:

```
Average processing time per frame: 40.00 ms (over 10 frames)
```

## Configuration

- Camera resolution is automatically set to match the model's input requirements
- Physical address mode is enabled for efficient memory handling
- RTSP streaming is available for remote viewing

## Dependencies

- SSCMA-Micro framework
- Sophgo camera components
- Cross-compilation toolchain for RISC-V