# sscma-supervisor

`sscma-supervisor` is a software supervisor application designed for recameraOS. It manages various services and functionalities related to the camera system, ensuring smooth operation and system stability.

## Features
- Supervises and controls the services of recameraOS.
- Manages device states, logs, and other essential functionalities.
- Provides a centralized management interface for monitoring and interacting with the system.

## Compilation Instructions

To compile `sscma-supervisor`, follow the steps below:

### 1. Clone the Repository
First, clone the repository and initialize the submodules:
```bash
git clone https://github.com/Seeed-Studio/sscma-example-sg200x
cd sscma-example-sg200x
git submodule update --init
cd solutions/supervisor
```

### 2. Configure the Build Options

The build process includes several configurable options:

- **WEB**: Default is `OFF`. To enable the web interface, set `WEB=ON`.
- **STAGING**: Default is `OFF`. To enable the staging (test) web interface, set `STAGING=ON`.
- **SG200X_SDK_PATH**: The path to the SG200X camera SDK. You need to replace this path with the location on your system.

### 3. Build the Project

After configuring your build options, use `cmake` to set up the build environment:

If you need the web interface or staging features, run:
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DWEB=ON -DSG200X_SDK_PATH=/path/to/your/sg200x_sdk -G Ninja ..
```

For a build without the web interface or staging features, run:
```bash
cmake -DCMAKE_BUILD_TYPE=Release -G Ninja ..
```

### 4. Compile the Application

Once the configuration is complete, compile the application with:
```bash
ninja
```

### 5. Package the Application

To package the application, navigate to the build directory and use `cpack`:
```bash
cd build && cpack
```

### 6. Install the Application

After packaging, transfer the `.deb` package to your device and install it using the `opkg` package manager:
```bash
opkg install supervisor-x.x.x.deb
```
