# Device Example

## Overview

**Device** is an example application demonstrating how to use the **SSCMA-Micro** framework to stream video from a camera using RTSP (Real-Time Streaming Protocol). This example shows how to initialize the device, configure the camera, and send video frames over RTSP.

## Getting Started

Before building this solution, ensure that you have set up the **ReCamera-OS** environment as described in the main project documentation:

🔗 **[SSCMA Example for SG200X - Main README](../../README.md)**

This includes:

- Setting up **ReCamera-OS**
- Configuring the SDK path
- Preparing the necessary toolchain

If you haven't completed these steps, follow the instructions in the main project README before proceeding.

## Building & Installing

### 1. Navigate to the `device` Solution

```bash
cd solutions/device
```

### 2. Build the Application

```bash
mkdir build
cd build
cmake ..
make
```

### 3. Run the Application

To run the application, use the following command:

```bash
./device
```

## Expected Output

Upon executing the application, the following occurs:

1. The device instance is created, and the RTSP transport is initialized with the specified configuration.
2. The application scans for available sensors and initializes the camera.
3. The camera is configured with parameters such as channel and window size.
4. The camera starts streaming, and video frames are retrieved in a loop.
5. Retrieved frames are sent over RTSP.

### Example Output Messages

During execution, you may see log messages indicating the size of the frames being processed:

```
frame size: 4096
```

The application continues to run until interrupted (e.g., by pressing `Ctrl+C`).

## Conclusion

This example serves as a basic introduction to using the SSCMA framework for streaming video from a camera. Users can modify the code and adapt it for their specific needs, experimenting with different camera settings and transport configurations.

For further details on the SSCMA framework, refer to the [SSCMA-Micro documentation](https://github.com/Seeed-Studio/SSCMA-Micro).
