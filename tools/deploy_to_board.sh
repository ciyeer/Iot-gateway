#!/bin/bash

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 <ip_address> [tar_file]"
    echo "Example: $0 192.168.2.88 build/aarch64/release/deploy/iotgw-xxxx-v0.1.0.tar.gz"
    echo "Example: $0 192.168.2.88"
    exit 1
fi

BOARD_IP=$1
TAR_FILE=$2

if [ -z "$TAR_FILE" ]; then
    TAR_FILE=$(ls -t build/aarch64/release/deploy/*.tar.gz 2>/dev/null | head -n 1)
fi

if [ -z "$TAR_FILE" ] || [ ! -f "$TAR_FILE" ]; then
    echo "Error: tar.gz package not found."
    exit 1
fi

echo "========================================"
echo "🚀 Deploying to RK3568 ($BOARD_IP)"
echo "📦 Artifact: $TAR_FILE"
echo "========================================"

# --- SSH Connection Multiplexing Setup ---
SSH_OPTS="-o ControlMaster=auto -o ControlPersist=60s -o StrictHostKeyChecking=no"
SSH_SOCKET="/tmp/ssh_mux_${BOARD_IP}_$$"

echo "🔌 Establishing SSH connection (Please enter password once)..."
# 启动 Master 连接
ssh -M -S "$SSH_SOCKET" -fN -o StrictHostKeyChecking=no "root@$BOARD_IP"

if [ $? -ne 0 ]; then
    echo "❌ Failed to establish SSH connection."
    exit 1
fi

# 注册清理函数：脚本退出时关闭 Master 连接
cleanup() {
    echo "🔌 Closing SSH connection..."
    ssh -S "$SSH_SOCKET" -O exit "root@$BOARD_IP" 2>/dev/null
    rm -f "$SSH_SOCKET"
}
trap cleanup EXIT

# 定义复用连接的命令别名
SSH_CMD="ssh -S $SSH_SOCKET"
SCP_CMD="scp -o ControlPath=$SSH_SOCKET"
# -----------------------------------------

# 1. Upload
echo "1️⃣ Uploading..."
$SCP_CMD "$TAR_FILE" "root@$BOARD_IP:/root/"

if [ $? -ne 0 ]; then
    echo "❌ Upload failed."
    exit 1
fi

# 2. Install (Unzip & Stop Old Process)
echo "2️⃣ Installing..."
$SSH_CMD "root@$BOARD_IP" /bin/sh <<EOF
    cd /root || exit 1
    
    FILENAME=\$(basename "$TAR_FILE")
    echo "Extracting \$FILENAME..."
    
    if tar --help 2>&1 | grep -q 'BusyBox'; then
        gunzip -c "\$FILENAME" | tar xf -
    else
        tar -xzf "\$FILENAME"
    fi
    
    if [ ! -d "iotgw_package" ]; then
        echo "❌ Error: iotgw_package directory not found after extraction!"
        exit 1
    fi
    
    echo "Stopping old process..."
    pkill -f iotgw_gateway || true
EOF

if [ $? -ne 0 ]; then
    echo "❌ Installation failed."
    exit 1
fi

# 3. Start Process (Background)
echo "3️⃣ Starting Gateway..."
$SSH_CMD "root@$BOARD_IP" /bin/sh <<'EOF'
    cd /root/iotgw_package || exit 1
    
    echo "--- Launching Gateway in Background ---"
    
    nohup ./bin/iotgw_gateway --yaml-config config/environments/deployment.yaml > logs/nohup.out 2>&1 < /dev/null &
    
    echo $! > logs/iotgw.pid
    sleep 1
EOF

# 4. Verify
echo "4️⃣ Verifying..."
$SSH_CMD "root@$BOARD_IP" /bin/sh <<'EOF'
    sleep 2
    
    if ps | grep -v grep | grep iotgw_gateway > /dev/null; then
        echo "✅ Gateway started successfully!"
        PID=$(ps | grep -v grep | grep iotgw_gateway | awk '{print $1}')
        echo "PID: $PID"
        echo "Logs: /root/iotgw_package/logs/iotgw.log"
    else
        echo "❌ Gateway failed to start. Check logs:"
        if [ -f /root/iotgw_package/logs/nohup.out ]; then
            tail -n 20 /root/iotgw_package/logs/nohup.out
        else
            echo "No nohup.out found."
        fi
        exit 1
    fi
EOF

echo "Done!"
