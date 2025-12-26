# Makefile for supervisor-go

# Build variables
VERSION ?= 1.0.0
BUILD_TIME := $(shell date -u '+%Y-%m-%d %H:%M:%S')
GIT_COMMIT := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")

# Go build flags
LDFLAGS := -ldflags "-s -w \
	-X 'main.Version=$(VERSION)' \
	-X 'main.BuildTime=$(BUILD_TIME)' \
	-X 'main.GitCommit=$(GIT_COMMIT)'"

# Output directory
BUILD_DIR := build
BIN_NAME := supervisor

# Cross-compilation for RISC-V
GOOS := linux
GOARCH := riscv64

# Directories
ROOTFS_DIR := rootfs
WWW_SRC_DIR := www
WWW_DST_DIR := $(ROOTFS_DIR)/usr/share/supervisor/www

# Default target
.PHONY: all
all: build

# Build for current platform
.PHONY: build
build:
	@echo "Building $(BIN_NAME) for $(shell go env GOOS)/$(shell go env GOARCH)..."
	@mkdir -p $(BUILD_DIR)
	CGO_ENABLED=0 go build $(LDFLAGS) -o $(BUILD_DIR)/$(BIN_NAME) ./cmd/supervisor

# Build for RISC-V (target device)
.PHONY: build-riscv
build-riscv:
	@echo "Cross-compiling $(BIN_NAME) for $(GOOS)/$(GOARCH)..."
	@mkdir -p $(BUILD_DIR)
	GOOS=$(GOOS) GOARCH=$(GOARCH) CGO_ENABLED=0 go build $(LDFLAGS) -o $(BUILD_DIR)/$(BIN_NAME)-riscv64 ./cmd/supervisor

# Build with CGO for RISC-V (requires cross-compiler)
.PHONY: build-riscv-cgo
build-riscv-cgo:
	@echo "Cross-compiling $(BIN_NAME) with CGO for $(GOOS)/$(GOARCH)..."
	@mkdir -p $(BUILD_DIR)
	CC=riscv64-linux-musl-gcc \
	GOOS=$(GOOS) GOARCH=$(GOARCH) CGO_ENABLED=1 \
	go build $(LDFLAGS) -o $(BUILD_DIR)/$(BIN_NAME)-riscv64-cgo ./cmd/supervisor

# Run tests
.PHONY: test
test:
	go test -v ./...

# Run tests with coverage
.PHONY: test-coverage
test-coverage:
	go test -v -coverprofile=coverage.out ./...
	go tool cover -html=coverage.out -o coverage.html

# Format code
.PHONY: fmt
fmt:
	go fmt ./...

# Lint code
.PHONY: lint
lint:
	golangci-lint run

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f coverage.out coverage.html

# Install dependencies
.PHONY: deps
deps:
	go mod download
	go mod tidy

# Generate (if needed)
.PHONY: generate
generate:
	go generate ./...

# Run locally
.PHONY: run
run:
	go run ./cmd/supervisor -p 8080 -r $(WWW_DST_DIR) -a

# Development mode with auto-reload (requires air)
.PHONY: dev
dev:
	air

# Build web frontend (requires npm)
.PHONY: www
www:
	@if [ -d "$(WWW_SRC_DIR)" ] && [ -f "$(WWW_SRC_DIR)/package.json" ]; then \
		echo "Building web frontend..."; \
		cd $(WWW_SRC_DIR) && npm install && npm run build; \
		mkdir -p $(WWW_DST_DIR); \
		cp -r $(WWW_SRC_DIR)/dist/* $(WWW_DST_DIR)/; \
	else \
		echo "Web frontend source not found at $(WWW_SRC_DIR)"; \
	fi

# Create release package (standalone)
.PHONY: package
package: build-riscv
	@echo "Creating release package..."
	@mkdir -p $(BUILD_DIR)/package/usr/local/bin
	@cp $(BUILD_DIR)/$(BIN_NAME)-riscv64 $(BUILD_DIR)/package/usr/local/bin/$(BIN_NAME)
	@if [ -d "$(ROOTFS_DIR)" ]; then \
		cp -r $(ROOTFS_DIR)/* $(BUILD_DIR)/package/; \
	fi
	@tar -czf $(BUILD_DIR)/supervisor-$(VERSION)-riscv64.tar.gz -C $(BUILD_DIR)/package .
	@echo "Package created: $(BUILD_DIR)/supervisor-$(VERSION)-riscv64.tar.gz"

# Build static binary (for maximum compatibility)
.PHONY: build-static
build-static:
	@echo "Building static $(BIN_NAME) for $(GOOS)/$(GOARCH)..."
	@mkdir -p $(BUILD_DIR)
	CGO_ENABLED=0 GOOS=$(GOOS) GOARCH=$(GOARCH) \
	go build -a $(LDFLAGS) -o $(BUILD_DIR)/$(BIN_NAME)-static ./cmd/supervisor

# Install locally (for development)
.PHONY: install
install: build
	@echo "Installing $(BIN_NAME) locally..."
	@mkdir -p ~/.local/bin
	@cp $(BUILD_DIR)/$(BIN_NAME) ~/.local/bin/

# Show help
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  build         - Build for current platform"
	@echo "  build-riscv   - Cross-compile for RISC-V (no CGO)"
	@echo "  build-riscv-cgo - Cross-compile for RISC-V with CGO"
	@echo "  build-static  - Build static binary for RISC-V"
	@echo "  test          - Run tests"
	@echo "  test-coverage - Run tests with coverage"
	@echo "  fmt           - Format code"
	@echo "  lint          - Lint code"
	@echo "  clean         - Clean build artifacts"
	@echo "  deps          - Install dependencies"
	@echo "  run           - Run locally"
	@echo "  dev           - Run with auto-reload"
	@echo "  www           - Build web frontend"
	@echo "  package       - Create release package"
	@echo "  install       - Install locally"
	@echo "  help          - Show this help"
