# QRCode Scanner Solution

## Overview

The **QRCode Scanner** solution is a C++ application for the **ReCamera** platform that scans and decodes QR codes from images. It uses **OpenCV** for image processing and **quirc** for QR code decoding. When executed, it reads a grayscale image file provided as a command-line argument and outputs the decoded QR code format and text to the terminal. This example serves as a starting point for developing QR code scanning applications within the **SSCMA Example for SG200X** framework.

**Note**: This solution requires **ReCameraOS 0.2.1 or higher** to function correctly.

## Getting Started

Before proceeding, ensure that you have set up the **ReCamera-OS** environment as described in the main project documentation:

🔗 **[SSCMA Example for SG200X - Main README](../../README.md)**

This includes:

- Setting up **ReCamera-OS** (version 0.2.1 or higher)
- Configuring the SDK path
- Preparing the necessary toolchain
- Installing **OpenCV** and **quirc** libraries

If you haven't done these steps yet, please follow the instructions in the main project README before proceeding.

## Dependencies

- **OpenCV**: For image loading and processing
- **quirc**: For QR code decoding
- **CMake**: For building the application
- **ReCameraOS 0.2.1 or higher**: Required for compatibility

Ensure these dependencies are installed and configured in your development environment.

## Building & Installing

### 1. Navigate to the `qrcode` Solution

```bash
cd solutions/qrcode
```

### 2. Build the Application

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

This will generate the executable in the `build` directory.

### 3. Package the Application

To generate an installable package:

```bash
cd build && cpack
```

This will create a `.deb` package for installation.

## Deploying & Running

### 1. Transfer the Package to Your Device

Copy the package to the ReCamera device using `scp`:

```bash
scp build/qrcode-1.0.0-1.deb recamera@192.168.42.1:/tmp/
```

Replace `recamera@192.168.42.1` with your device's IP address.

### 2. Install the Package

SSH into the device and install the package:

```bash
ssh recamera@192.168.42.1
sudo opkg install /tmp/qrcode-1.0.0-1.deb
```

### 3. Run the Application

Once installed, execute the application with an image file containing a QR code:

```bash
qrcode /path/to/qr_code_image.png
```

Replace `/path/to/qr_code_image.png` with the path to your QR code image file on the device.

#### Expected Output

```bash
Barcode format: QR_CODE
Barcode text: <decoded_text>
```

If the image does not contain a valid QR code or cannot be loaded, an error message will be displayed.

## Directory Structure

```
qrcode/
├── CMakeLists.txt    # CMake build configuration
├── main              # Main application file
└── README.md         # This README file
```

## Notes

- The application expects a grayscale image as input. Ensure the provided image is clear and contains a valid QR code for successful decoding.
- The **quirc** library is optimized for QR code decoding, but it is specifically configured for QR codes in this example.
- For troubleshooting, ensure that the image file is accessible on the device and that the **OpenCV** and **quirc** libraries are properly linked during the build process.
