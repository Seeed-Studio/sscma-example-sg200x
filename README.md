# SSCMA Example for SG200X

This repository provides a compilation framework to support running applications on the ReCamera platform.

## Prerequisites

This project depends on the **reCamera-OS**. Please make sure to clone and set up the reCamera-OS from the following repository:

[ReCamera-OS GitHub Repository](https://github.com/Seeed-Studio/reCamera-OS)

## Compilation Instructions

Follow the steps below to compile and package the project:

### 1. Clone the Repository
```bash
git clone https://github.com/Seeed-Studio/sscma-example-sg200x
cd sscma-example-sg200x
git submodule update --init
```

### 2. Set the Necessary Paths

- Set the SDK path:
  ```bash
  export SG200X_SDK_PATH=<PATH_TO_RECAMERA-OS>/output/sg2002_recamera_emmc/
  ```

- Set the compiler path:
  ```bash
  export PATH=<PATH_TO_RECAMERA-OS>/host-tools/gcc/riscv64-linux-musl-x86_64/bin:$PATH
  ```

### 3. Build the Application

Navigate to the solution directory, configure the build, and compile the project:

```bash
cd solutions/helloworld
mkdir build && cd build
cmake ..
make
```

### 4. Package the Application

To package the application, run the following command in the build directory:

```bash
cpack
```