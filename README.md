# SSCMA Example for SG200X  

This repository provides a compilation framework for developing and running applications on the **ReCamera** platform. It includes setup instructions, compilation steps, and installation guidelines.  

## Project Directory Structure  

```bash
.
├── cmake         # Build scripts
├── components    # Functional components
├── docs          # Documentation
├── images        # Images
├── scripts       # Scripts
├── solutions     # Applications
├── test          # Tests
└── tools         # Tools
```

## Prerequisites  

### 1. Clone and Set Up **ReCamera-OS**  

This project depends on **ReCamera-OS**, which provides the necessary toolchain, SDK, and runtime environment. Ensure you have cloned and set up **ReCamera-OS** from the following repository:  

🔗 [ReCamera-OS GitHub Repository](https://github.com/Seeed-Studio/reCamera-OS)  

```bash
git clone https://github.com/Seeed-Studio/reCamera-OS.git
cd reCamera-OS
# Follow the setup instructions in the repository
```  

### 2. Use a Prebuilt SDK (Optional)  

If you do not wish to build **ReCamera-OS** manually, you can download a prebuilt SDK package:  

1. Visit [ReCamera-OS Releases](https://github.com/Seeed-Studio/reCamera-OS/releases).  
2. Download the latest **reCamera_OS_SDK_x.x.x.tar.gz** package.  
3. Extract the package and set the SDK path:  

   ```bash
   export SG200X_SDK_PATH=<PATH_TO_RECAMERA-OS-SDK>/sg2002_recamera_emmc/
   ```  

## Compilation Guide  

Follow these steps to set up the environment, compile the project, and generate the necessary application package.  

### 1. Clone This Repository  

```bash
git clone https://github.com/Seeed-Studio/sscma-example-sg200x
cd sscma-example-sg200x
git submodule update --init
```  

### 2. Configure Environment  

Set up the required paths before building the application:  

```bash
export SG200X_SDK_PATH=<PATH_TO_RECAMERA-OS>/output/sg2002_recamera_emmc/
export PATH=<PATH_TO_RECAMERA-OS>/host-tools/gcc/riscv64-linux-musl-x86_64/bin:$PATH
```  

### 3. Build the Application  

Navigate to the project directory and compile:  

```bash
cd solutions/helloworld
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```  

If the build process completes successfully, the executable binary should be available in the `build` directory.  

### 4. Package the Application  

To prepare the application for distribution, package it using `cpack`:  

```bash
cd build && cpack
```  

This will generate a **.deb** package, which can be installed on the device.  

## Deploying the Application  

### 1. Transfer the Package to the Device  

Use **scp** or other file transfer methods to copy the package to the ReCamera device:  

```bash
scp build/helloworld-1.0.0-1.deb recamera@192.168.42.1:/tmp/
```  

Replace `recamera@192.168.42.1` with the actual username and IP address of your device.  

### 2. Install the Application  

Log into the device via SSH and install the package using `opkg`:  

```bash
ssh recamera@192.168.42.1
sudo opkg install /tmp/helloworld-1.0.0-1.deb
```  

**Note**: sudo password is the same as the WEB UI password. default is `recamera`.

### 3. Run the Application  

Once installed, you can run the application:  

```bash
helloworld
Hello, ReCamera!
```  

For more information, go to the specific solution's README.