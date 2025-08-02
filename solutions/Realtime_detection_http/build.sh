#!/bin/bash
export SG200X_SDK_PATH=$HOME/recamera/sg2002_recamera_emmc/
export PATH=$HOME/recamera/host-tools/gcc/riscv64-linux-musl-x86_64/bin:$PATH

# SSH configuration
TARGET_HOST="192.168.42.1"
TARGET_USER="recamera"
# Load password from environment variable or prompt user
TARGET_PASSWORD="${RECAMERA_PASSWORD:-}"

if [ -z "$TARGET_PASSWORD" ]; then
    echo "Please set RECAMERA_PASSWORD environment variable or it will be prompted when needed"
fi

# Function to setup SSH key if it doesn't exist
setup_ssh_key() {
    echo "Setting up SSH key authentication..."
    
    # Generate SSH key if it doesn't exist
    if [ ! -f ~/.ssh/id_rsa ]; then
        echo "Generating SSH key..."
        ssh-keygen -t rsa -b 2048 -f ~/.ssh/id_rsa -N ""
    fi
    
    # Copy SSH key to target device using sshpass
    if [ -z "$TARGET_PASSWORD" ]; then
        echo "No password set. Please enter password manually when prompted."
        ssh-copy-id -o StrictHostKeyChecking=no "$TARGET_USER@$TARGET_HOST"
    elif command -v sshpass >/dev/null 2>&1; then
        echo "Copying SSH key to target device..."
        sshpass -p "$TARGET_PASSWORD" ssh-copy-id -o StrictHostKeyChecking=no "$TARGET_USER@$TARGET_HOST"
    else
        echo "sshpass not found. Installing..."
        sudo apt-get update && sudo apt-get install -y sshpass
        echo "Copying SSH key to target device..."
        sshpass -p "$TARGET_PASSWORD" ssh-copy-id -o StrictHostKeyChecking=no "$TARGET_USER@$TARGET_HOST"
    fi
}

# Function to execute SSH commands with key or password fallback
ssh_exec() {
    local cmd="$1"
    # Try with SSH key first
    if ssh -o BatchMode=yes -o ConnectTimeout=5 "$TARGET_USER@$TARGET_HOST" "$cmd" 2>/dev/null; then
        return 0
    else
        # Fallback to password if key auth fails
        if [ -n "$TARGET_PASSWORD" ] && command -v sshpass >/dev/null 2>&1; then
            sshpass -p "$TARGET_PASSWORD" ssh -o StrictHostKeyChecking=no "$TARGET_USER@$TARGET_HOST" "$cmd"
        else
            echo "SSH key auth failed. Please enter password manually:"
            ssh "$TARGET_USER@$TARGET_HOST" "$cmd"
        fi
    fi
}

# Function to execute SCP with key or password fallback
scp_exec() {
    local src="$1"
    local dst="$2"
    # Try with SSH key first
    if scp -o BatchMode=yes -o ConnectTimeout=5 "$src" "$TARGET_USER@$TARGET_HOST:$dst" 2>/dev/null; then
        return 0
    else
        # Fallback to password if key auth fails
        if [ -n "$TARGET_PASSWORD" ] && command -v sshpass >/dev/null 2>&1; then
            sshpass -p "$TARGET_PASSWORD" scp -o StrictHostKeyChecking=no "$src" "$TARGET_USER@$TARGET_HOST:$dst"
        else
            echo "SSH key auth failed. Please enter password manually:"
            scp "$src" "$TARGET_USER@$TARGET_HOST:$dst"
        fi
    fi
}

#test if build folder exists
if [ ! -d "build" ]; then
    mkdir build
fi

cd build
cmake ..
make -j8

# Check if SSH key authentication is working
echo "Testing SSH connection..."
if ! ssh -o BatchMode=yes -o ConnectTimeout=5 "$TARGET_USER@$TARGET_HOST" "echo 'SSH key auth working'" 2>/dev/null; then
    echo "SSH key authentication not working. Setting up..."
    setup_ssh_key
fi

# Create www directory on target if it doesn't exist
echo "Creating www directory on target..."
ssh_exec "mkdir -p ~/www"

# Copy the built binary to the target device
echo "Copying binary to target device..."
scp_exec "Realtime_detection_http" "audio_test"

# Copy the HTML interface to the target device
echo "Copying HTML interface to target device..."
scp_exec "../www/index.html" "~/www/index.html"

# SSH into the device
echo "Connecting to target device..."
if ssh -o BatchMode=yes -o ConnectTimeout=5 "$TARGET_USER@$TARGET_HOST" 2>/dev/null; then
    ssh "$TARGET_USER@$TARGET_HOST"
else
    if [ -n "$TARGET_PASSWORD" ] && command -v sshpass >/dev/null 2>&1; then
        sshpass -p "$TARGET_PASSWORD" ssh -o StrictHostKeyChecking=no "$TARGET_USER@$TARGET_HOST"
    else
        echo "Please enter password when prompted:"
        ssh "$TARGET_USER@$TARGET_HOST"
    fi
fi
