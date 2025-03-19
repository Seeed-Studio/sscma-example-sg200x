# HelloWorld Solution  

## Overview  

The **HelloWorld** solution is a simple application for the **ReCamera** platform. When executed, it prints **"Hello, ReCamera!"** to the terminal. This example serves as a starting point for developing applications within the **SSCMA Example for SG200X** framework.  

## Getting Started  

Before proceeding, ensure that you have set up the **ReCamera-OS** environment as described in the main project documentation:  

🔗 **[SSCMA Example for SG200X - Main README](../../README.md)**  

This includes:  

- Setting up **ReCamera-OS**  
- Configuring the SDK path  
- Preparing the necessary toolchain  

If you haven't done these steps yet, please follow the instructions in the main project README before proceeding.

## Building & Installing  

### 1. Navigate to the `helloworld` Solution  

```bash
cd solutions/helloworld
```

### 2. Build the Application  

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

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
scp build/helloworld-1.0.0-1.deb recamera@192.168.42.1:/tmp/
```

Replace `recamera@192.168.42.1` with your device's IP address.

### 2. Install the Package  

SSH into the device and install the package:  

```bash
ssh recamera@192.168.42.1
sudo opkg install /tmp/helloworld-1.0.0-1.deb
```

### 3. Run the Application  

Once installed, execute:  

```bash
helloworld
```

#### Expected Output  

```bash
Hello, ReCamera!
```

## Directory Structure  

```
helloworld/
├── CMakeLists.txt    # CMake build configuration
├── src               # Source code directory
│   └── main.c        # Main application file
├── build             # Build output (generated after compilation)
└── README.md         # This README file
```
