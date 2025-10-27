# Model Detector

## Overview

**model_detector** is an example application demonstrating how to combine real-time camera capture with AI model inference using the **SSCMA-Micro** framework. This application captures video frames from the camera, performs object detection using a pre-trained model, and displays the results with timing statistics.

## Getting Started

Before building this solution, ensure that you have set up the **ReCamera-OS** environment as described in the main project documentation:

🔗 **[SSCMA Example for SG200X - Main README](../../README.md)**

This includes:

- Setting up **ReCamera-OS**
- Configuring the SDK path
- Preparing the necessary toolchain

If you haven't completed these steps, follow the instructions in the main project README before proceeding.

## Building & Installing

### 1. Navigate to the `model_detector` Solution

```bash
cd solutions/model_detector
```

### 2. Build the Application

```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../../cmake/toolchain-riscv64-linux-musl-x86_64.cmake
make
```

### 3. Run the Application

To run the application, use the following command:

```bash
./model_detector <model_file> [<confidence_threshold>]
```

- `<model_file>`: Path to the model file (e.g., `yolo11n_detection_cv181x_int8.cvimodel`).
- `<confidence_threshold>` (optional): Minimum confidence for detections (default: 0.5).

**Example:**

```bash
./model_detector yolo11n_detection_cv181x_int8.cvimodel 0.7
```

## Expected Output

Upon executing the application, the following occurs:

1. The specified model is loaded.
2. The camera starts capturing frames in real-time.
3. For each frame, inference is performed, and detection results are displayed.
4. Timing statistics are shown for capture, inference, and total processing time.
5. Detection results include the number of objects found and their bounding boxes with confidence scores.

### Example Output Messages

```
Frame 1: Capture: 15 ms, Inference: 25 ms, Total: 48 ms
Detections: 2 objects found
- Class 0: 0.85 confidence at [100,200,150,250]
- Class 1: 0.72 confidence at [300,400,350,450]
```

Every 10 frames:

```
Average processing time per frame: 50.00 ms (over 10 frames)
```

## Configuration

- Camera resolution is automatically set to match the model's input requirements
- Physical address mode is enabled for efficient memory handling
- RTSP streaming is available for remote viewing

## Conclusion

This example serves as a basic introduction to real-time object detection using the SSCMA framework with camera input. Users can modify the code and adapt it for their specific needs, experimenting with different models and thresholds.

For further details on the SSCMA framework, refer to the [SSCMA-Micro documentation](https://github.com/Seeed-Studio/SSCMA-Micro).