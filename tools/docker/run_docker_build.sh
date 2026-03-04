#!/bin/bash

# 获取脚本所在目录的绝对路径
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

IMAGE_NAME="iotgw-builder:latest"

# 0. 检查 Docker 是否运行
if ! command -v docker &> /dev/null; then
    echo "Error: Docker is not installed. Please install Docker Desktop for Mac."
    exit 1
fi

if ! docker info &> /dev/null; then
    echo "Error: Docker daemon is not running. Please start Docker Desktop."
    exit 1
fi

# 1. 构建 Docker 镜像
echo "Building Docker image..."
docker build -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile.build" "$SCRIPT_DIR"

if [ $? -ne 0 ]; then
    echo "Docker build failed!"
    exit 1
fi

# 获取版本信息 (仅用于包名生成)
cd "$PROJECT_ROOT"
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
VERSION=$(grep "project(IotEdgeGateway VERSION" IotEdgeGateway/CMakeLists.txt | awk '{print $3}' | tr -d ')')
if [ -z "$VERSION" ]; then VERSION="0.0.0"; fi

echo "=================================================="
echo "🔥 IotGateway CI Build Pipeline"
echo "=================================================="
echo "Version: $VERSION"
echo "Commit : $GIT_COMMIT"
echo "Arch   : aarch64 (Cross Compile)"
echo "Mode   : Release"
echo "=================================================="

# 2. 运行容器进行编译和打包
echo "Running build in Docker container..."

docker run --rm -v "$PROJECT_ROOT:/workspace" "$IMAGE_NAME" bash -c "
    set -e # 遇到错误立即退出
    
    BUILD_DIR=\"build/aarch64/release\"
    DEPLOY_DIR=\"\$BUILD_DIR/deploy\"
    PACKAGE_NAME=\"iotgw_package\"
    
    echo '--- [1/3] Configuring CMake ---'
    # 注意：版本号和 Git Commit 现由 CMake 自动获取和注入 (via version.hpp)
    cmake -S IotEdgeGateway -B \$BUILD_DIR \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/workspace/tools/docker/toolchain-aarch64.cmake
        
    echo '--- [2/3] Building & Stripping ---'
    # Strip 已集成在 CMake POST_BUILD 中
    cmake --build \$BUILD_DIR
    
    echo '--- [3/3] Packaging ---'
    # 清理旧包
    rm -rf \$DEPLOY_DIR
    mkdir -p \$DEPLOY_DIR/\$PACKAGE_NAME/bin
    mkdir -p \$DEPLOY_DIR/\$PACKAGE_NAME/config
    mkdir -p \$DEPLOY_DIR/\$PACKAGE_NAME/www
    mkdir -p \$DEPLOY_DIR/\$PACKAGE_NAME/logs
    
    # 复制文件
    cp \$BUILD_DIR/iotgw_gateway \$DEPLOY_DIR/\$PACKAGE_NAME/bin/
    cp -r IotEdgeGateway/config/* \$DEPLOY_DIR/\$PACKAGE_NAME/config/
    cp -r IotEdgeGateway/www/* \$DEPLOY_DIR/\$PACKAGE_NAME/www/
    
    # 自动生成部署配置
    DEPLOY_YAML=\$DEPLOY_DIR/\$PACKAGE_NAME/config/environments/deployment.yaml
    cp IotEdgeGateway/config/environments/development.yaml \$DEPLOY_YAML
    sed -i 's|config_root: IotEdgeGateway/config|config_root: config|g' \$DEPLOY_YAML
    sed -i 's|www_root: .*|www_root: www|g' \$DEPLOY_YAML
    sed -i 's|log_file: .*|log_file: logs/iotgw.log|g' \$DEPLOY_YAML
    
    # 创建启动脚本
    cat > \$DEPLOY_DIR/\$PACKAGE_NAME/start.sh <<EOF
#!/bin/bash
cd \"\$(dirname \"\$0\")\"
mkdir -p logs
export LD_LIBRARY_PATH=./lib:\$LD_LIBRARY_PATH
./bin/iotgw_gateway --yaml-config config/environments/deployment.yaml
EOF
    chmod +x \$DEPLOY_DIR/\$PACKAGE_NAME/start.sh
    
    echo '--- Compressing Artifacts ---'
    TAR_NAME=\"iotgw-${GIT_COMMIT}-v${VERSION}.tar.gz\"
    cd \$DEPLOY_DIR
    tar -czf \$TAR_NAME \$PACKAGE_NAME
    
    echo \"Build Success!\"
    echo \"Package: \$DEPLOY_DIR/\$TAR_NAME\"
"

echo "Done! Artifacts are ready in $PROJECT_ROOT/build/aarch64/release/deploy"
