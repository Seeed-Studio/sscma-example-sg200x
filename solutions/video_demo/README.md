# Video Demo (RTSP Streaming)

## Overview

**Video Demo** captures video frames from the camera and streams H.264 video over RTSP (Real-Time Streaming Protocol) using the Sophgo SDK.

## Getting Started

Before building this solution, ensure that you have set up the **Sophgo SDK** environment as described in the main project documentation.

## Building & Installing

### 1. Navigate to the Solution

```bash
cd solutions/video_demo
```

### 2. Build the Application

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

### 3. Run the Application

```bash
./video_demo
```

## Expected Output

The application sets up a single video channel:

- **Channel 2**: H.264 format, 1920x1080, 30 FPS → RTSP streaming

The camera starts capturing, and encoded frames are streamed over RTSP. The application runs until interrupted (`Ctrl+C`).

### Example Output

```
start save file to /userdata/local/VENC0_0.h264
```

## Architecture

```
Camera
  └─ Channel 2: H.264 1920x1080 @ 30 FPS → RTSP streaming
```

Note: Channel 0 (RGB) and Channel 1 (NV21) configurations are present in the source code but commented out. They can be enabled for frame-saving to disk.

## Directory Structure

```
video_demo/
├── CMakeLists.txt
├── main/
│   └── main.cpp
└── README.md
```
