# Supervisor (Go Version)

A secure HTTP supervisor daemon for reCamera devices, rewritten in Go for improved security and performance. This is a **standalone** replacement for the C++ supervisor - it includes all necessary source files and resources.

## Development Workflow

### Prerequisites

This project uses [Nix](https://nixos.org/) for reproducible development environments.

### Quick Start

```bash
# Enter the development environment
nix develop

# Build the opkg package (includes frontend and backend)
cd solutions/supervisor
make opkg

# Deploy to device
scp build/supervisor_1.0.0_riscv64.ipk recamera@192.168.10.115:/userdata

# On the device, install the package
ssh recamera@192.168.10.115
sudo opkg install /userdata/supervisor_1.0.0_riscv64.ipk
```

### Build Commands

| Command | Description |
|---------|-------------|
| `make opkg` | Build complete opkg package (frontend + backend) for deployment |
| `make build` | Build binary for current platform |
| `make build-riscv` | Cross-compile for RISC-V (no CGO) |
| `make build-riscv-cgo` | Cross-compile for RISC-V with CGO (requires cross-compiler) |
| `make build-static` | Build static binary for RISC-V |
| `make www` | Build web frontend only |
| `make package` | Create release tarball package |
| `make test` | Run tests |
| `make test-coverage` | Run tests with coverage report |
| `make fmt` | Format Go code |
| `make lint` | Lint code (requires golangci-lint) |
| `make deps` | Install/update Go dependencies |
| `make run` | Run locally with auth disabled |
| `make dev` | Run with auto-reload (requires air) |
| `make install` | Install binary to ~/.local/bin |
| `make clean` | Clean build artifacts |
| `make help` | Show all available targets |

## Why Go?

The original C++ supervisor had several security concerns that are addressed in this Go rewrite:

### Security Improvements

1. **Removed Hardcoded AES Key**: The original code had `KEY_AES_128 = "zqCwT7H7!rNdP3wL"` hardcoded. The Go version uses:
   - JWT tokens with cryptographically random secrets generated at runtime
   - HMAC-SHA256 signing
   - Configurable via environment variable (`SUPERVISOR_JWT_SECRET`)

2. **Token Security**:
   - JWT tokens with proper expiration
   - HMAC-SHA256 signing
   - No hardcoded secrets in source code

3. **Rate Limiting**:
   - Built-in login attempt limiting
   - Account lockout after too many failed attempts
   - Per-IP request rate limiting

4. **Input Validation**:
   - Path traversal prevention
   - Proper input sanitization
   - Shell command injection prevention via argument escaping

5. **Security Headers**:
   - X-Frame-Options (clickjacking protection)
   - X-Content-Type-Options (MIME type sniffing prevention)
   - X-XSS-Protection
   - Content-Security-Policy
   - Referrer-Policy

6. **Memory Safety**:
   - Go's garbage collection eliminates memory leaks
   - No buffer overflows
   - Safe string handling

## Features

- HTTP/HTTPS server with TLS support
- RESTful API for device management
- User authentication and authorization
- WiFi network management
- File system operations (with security restrictions)
- LED control
- System power management
- Model file management
- Platform configuration

## Building

### Prerequisites

- Go 1.21 or later
- Node.js and npm (for web frontend, optional)
- For cross-compilation: RISC-V musl cross-compiler (optional, for CGO)

### Build for Current Platform

```bash
make build
```

### Build for Target Device (RISC-V)

```bash
# Static binary (recommended)
make build-static

# Or standard build
make build-riscv
```

### Build Web Frontend

```bash
make www
```

### Using CMake

```bash
mkdir build && cd build
cmake -DTARGET_ARCH=riscv64 ..
make
```

## Installation

### Quick Install

```bash
# Build the package
make package

# Copy to device
scp build/supervisor-1.0.0-riscv64.tar.gz recamera@192.168.42.1:/tmp/

# On the device
ssh recamera@192.168.42.1
cd /
sudo tar -xzf /tmp/supervisor-1.0.0-riscv64.tar.gz
```

### Manual Installation

1. Copy the binary to `/usr/local/bin/supervisor`
2. Copy scripts to `/usr/share/supervisor/scripts/`
3. Copy web files to `/usr/share/supervisor/www/`
4. Install the init script to `/etc/init.d/S93sscma-supervisor`

## Usage

```bash
supervisor [options]

Options:
  -v            Show version
  -B            Run as daemon
  -r <dir>      Set root directory (default: /usr/share/supervisor/www/)
  -p <port>     Set HTTP port (default: 80)
  -s <script>   Set script path (default: /usr/share/supervisor/scripts/main.sh)
  -a            Disable authentication
  -D <level>    Log level (0=Error, 1=Warning, 2=Info, 3=Debug, 4=Verbose)
```

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `SUPERVISOR_HTTP_PORT` | HTTP port | 80 |
| `SUPERVISOR_HTTPS_PORT` | HTTPS port | (disabled) |
| `SUPERVISOR_ROOT_DIR` | Web root directory | /usr/share/supervisor/www/ |
| `SUPERVISOR_SCRIPT_PATH` | Main script path | /usr/share/supervisor/scripts/main.sh |
| `SUPERVISOR_NO_AUTH` | Disable authentication | false |
| `SUPERVISOR_JWT_SECRET` | JWT signing secret | (auto-generated) |
| `SUPERVISOR_LOG_LEVEL` | Log level (0-4) | 1 (Warning) |
| `SUPERVISOR_LOCAL_DIR` | Local storage directory | /userdata |
| `SUPERVISOR_SD_DIR` | SD card mount point | /mnt/sd |

## API Endpoints

### Authentication

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/version` | GET | No | Get version info |
| `/api/userMgr/login` | POST | No | User login |
| `/api/userMgr/queryUserInfo` | GET | No | Get user info |
| `/api/userMgr/updatePassword` | POST | No | Update password |
| `/api/userMgr/setSShStatus` | POST | Yes | Enable/disable SSH |
| `/api/userMgr/addSShkey` | POST | Yes | Add SSH key |
| `/api/userMgr/deleteSShkey` | POST | Yes | Delete SSH key |

### Device Management

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/deviceMgr/queryDeviceInfo` | GET | No | Get device info |
| `/api/deviceMgr/getDeviceInfo` | GET | Yes | Get detailed device info |
| `/api/deviceMgr/updateDeviceName` | POST | Yes | Update device name |
| `/api/deviceMgr/setPower` | POST | Yes | Set power mode |
| `/api/deviceMgr/getModelList` | GET | No | List available models |
| `/api/deviceMgr/uploadModel` | POST | No | Upload a model file |

### WiFi Management

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/wifiMgr/getWiFiInfoList` | GET | Yes | Get WiFi networks |
| `/api/wifiMgr/connectWiFi` | POST | Yes | Connect to WiFi |
| `/api/wifiMgr/disconnectWiFi` | POST | Yes | Disconnect from WiFi |
| `/api/wifiMgr/forgetWiFi` | POST | Yes | Forget WiFi network |
| `/api/wifiMgr/switchWiFi` | POST | Yes | Enable/disable WiFi |

### File Management

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/fileMgr/list` | GET | Yes | List directory |
| `/api/fileMgr/mkdir` | POST | Yes | Create directory |
| `/api/fileMgr/remove` | POST | Yes | Remove file/directory |
| `/api/fileMgr/upload` | POST | Yes | Upload file |
| `/api/fileMgr/download` | GET | Yes | Download file |
| `/api/fileMgr/rename` | POST | Yes | Rename file/directory |
| `/api/fileMgr/info` | GET | Yes | Get file/directory info |

### LED Management

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/ledMgr/getLEDs` | GET | Yes | Get all LEDs |
| `/api/ledMgr/getLED` | GET | Yes | Get LED info |
| `/api/ledMgr/setLED` | POST | Yes | Set LED brightness/trigger |
| `/api/ledMgr/getLEDTriggers` | GET | Yes | Get LED triggers |

## Development

### Running Tests

```bash
make test
```

### Running Locally

```bash
# Run with authentication disabled for development
make run
```

### Code Formatting

```bash
make fmt
```

## Project Structure

```
supervisor-go/
├── cmd/
│   └── supervisor/
│       └── main.go           # Entry point
├── internal/
│   ├── api/
│   │   └── response.go       # API response helpers
│   ├── auth/
│   │   └── auth.go           # Authentication (JWT, bcrypt)
│   ├── config/
│   │   └── config.go         # Configuration management
│   ├── handler/
│   │   ├── device.go         # Device API handlers
│   │   ├── file.go           # File API handlers
│   │   ├── led.go            # LED API handlers
│   │   ├── user.go           # User API handlers
│   │   └── wifi.go           # WiFi API handlers
│   ├── middleware/
│   │   └── middleware.go     # HTTP middleware
│   ├── script/
│   │   └── script.go         # Script execution
│   └── server/
│       └── server.go         # HTTP server
├── pkg/
│   └── logger/
│       └── logger.go         # Logging package
├── rootfs/                   # Root filesystem resources
│   ├── etc/
│   │   ├── avahi/            # Avahi service config
│   │   └── init.d/           # Init scripts
│   └── usr/share/supervisor/
│       ├── flows.json        # Node-RED flows
│       ├── models/           # Pre-installed models
│       └── scripts/          # Shell scripts
├── www/                      # Web frontend source
│   ├── src/                  # React/TypeScript source
│   └── package.json          # NPM dependencies
├── control/                  # Package control scripts
├── CMakeLists.txt            # CMake build configuration
├── Makefile                  # Make build configuration
├── go.mod                    # Go modules
└── README.md                 # This file
```

## License

See the [LICENSE](../../LICENSE) file for details.

## Comparison with C++ Version

| Feature | C++ Version | Go Version |
|---------|------------|------------|
| Authentication | Hardcoded AES key | JWT with random secrets |
| Token signing | None | HMAC-SHA256 |
| Rate limiting | None | Built-in |
| Security headers | None | Full suite |
| Memory safety | Manual | Automatic (GC) |
| Cross-compilation | Complex (toolchain) | Simple (`GOOS/GOARCH`) |
| Static binary | Requires musl | Native support |
| Dependencies | Mongoose, OpenSSL, etc. | Minimal (stdlib + 2 libs) |

The Go version is a complete standalone replacement that includes all necessary resources (scripts, models, web UI source, init scripts).
