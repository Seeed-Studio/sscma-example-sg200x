# QR Code Reader (Real-time Camera + QR Decoding)

## Overview

**qrcode-reader** captures video frames from the camera, decodes QR codes in real-time using the **quirc** library, and streams H.264 video over RTSP.

QR decoding runs on every 30th frame from a 640x640 RGB channel, performing 10-iteration benchmarking for average decode time.

## Getting Started

Before proceeding, ensure that you have set up the **ReCamera-OS** environment as described in the main project documentation:

🔗 **[SSCMA Example for SG200X - Main README](../../README.md)**

## Building & Installing

### 1. Navigate to the Solution

```bash
cd solutions/qrcode-reader
```

### 2. Build the Application

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

### 3. Package the Application

```bash
cd build && cpack
```

## Deploying & Running

### 1. Transfer the Package to Your Device

```bash
scp build/qrcode-reader-1.0.0-1.deb recamera@192.168.42.1:/tmp/
```

### 2. Install the Package

```bash
ssh recamera@192.168.42.1
sudo opkg install /tmp/qrcode-reader-1.0.0-1.deb
```

### 3. Run the Application

```bash
qrcode-reader
```

#### Expected Output

```
QR code text: https://example.com, Average decode time: 12.50 ms
```

The application runs continuously, scanning camera frames for QR codes and streaming video over RTSP.

## Architecture

```
Camera
  ├─ Channel 0: RGB888 640x640 @ 1 FPS → QR code detection (quirc, every 30th frame)
  └─ Channel 2: H.264 1920x1080 @ 30 FPS → RTSP streaming
```

## Directory Structure

```
qrcode-reader/
├── CMakeLists.txt
├── main/
│   └── main.cpp
└── README.md
```
