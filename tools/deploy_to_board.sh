#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <ip_address> <tar_file>"
    echo "Example: $0 192.168.1.88 build/aarch64/release/deploy/iotgw-xxxx-v0.1.0.tar.gz"
    exit 1
fi

BOARD_IP=$1
TAR_FILE=$2

if [ ! -f "$TAR_FILE" ]; then
    echo "Error: File $TAR_FILE not found!"
    exit 1
fi

echo "========================================"
echo "🚀 Deploying to RK3568 ($BOARD_IP)"
echo "📦 Artifact: $TAR_FILE"
echo "========================================"

# 1. Upload
echo "1️⃣ Uploading..."
scp "$TAR_FILE" "root@$BOARD_IP:/root/"

if [ $? -ne 0 ]; then
    echo "❌ Upload failed. Check connection and SSH keys."
    exit 1
fi

# 2. Install & Restart
echo "2️⃣ Installing & Restarting..."
ssh "root@$BOARD_IP" "
    cd /root
    # 解压
    tar -xzf $(basename "$TAR_FILE")
    
    # 进入目录
    cd iotgw_package
    
    # 停止旧进程 (如果存在)
    pkill -f iotgw_gateway || true
    
    # 启动新进程
    nohup ./start.sh > /dev/null 2>&1 &
    
    echo '✅ Deployed & Started!'
    ps aux | grep iotgw_gateway | grep -v grep
"

echo "Done!"
