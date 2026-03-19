#!/bin/bash

# 获取脚本所在目录的绝对路径
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

IMAGE_NAME="iotgw-builder:latest"

# 尝试添加 Docker 常见路径到 PATH
export PATH=$PATH:/usr/local/bin:/Applications/Docker.app/Contents/Resources/bin

# 0. 检查 Docker 是否运行 (简化版)
if ! command -v docker &> /dev/null; then
    echo "Error: Docker is not installed or not in PATH."
    echo "Try adding /usr/local/bin or /Applications/Docker.app/Contents/Resources/bin to PATH."
    exit 1
fi

if ! docker info &> /dev/null; then
    echo "Error: Docker daemon is not running. Please start Docker Desktop."
    exit 1
fi

if ! docker info &> /dev/null; then
    echo "Error: Docker daemon is not running. Please start Docker Desktop."
    exit 1
fi

# 1. 构建 Docker 镜像
echo "Building Docker image..."
# 使用阿里云镜像源加速构建
docker build -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR"

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

# 使用外部脚本 entrypoint_build.sh 来避免 bash -c 复杂的转义问题
# 我们在这里动态生成这个脚本，然后挂载进去
cat > "$SCRIPT_DIR/entrypoint_build.sh" <<'EOF'
#!/bin/bash
set -e # 遇到错误立即退出

PROJECT_ROOT="/workspace"
BUILD_DIR="${PROJECT_ROOT}/build/aarch64/release"
DEPLOY_DIR="${BUILD_DIR}/deploy"
PACKAGE_NAME="iotgw_package"

# 获取 Git 信息
cd "${PROJECT_ROOT}"
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
VERSION=$(grep "project(IotEdgeGateway VERSION" IotEdgeGateway/CMakeLists.txt | awk '{print $3}' | tr -d ')')
if [ -z "$VERSION" ]; then VERSION="0.0.0"; fi

echo "=================================================="
echo "🔥 IotGateway CI Build Pipeline (Internal)"
echo "=================================================="
echo "Version: $VERSION"
echo "Commit : $GIT_COMMIT"
echo "Arch   : aarch64 (Cross Compile)"
echo "Mode   : Release"
echo "=================================================="

# 1. 配置 CMake
echo '--- [1/3] Configuring CMake ---'
cmake -S IotEdgeGateway -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${PROJECT_ROOT}/scripts/docker/toolchain-aarch64.cmake"

# 2. 编译 & Strip
echo '--- [2/3] Building & Stripping ---'
cmake --build "${BUILD_DIR}"

# 3. 打包
echo '--- [3/3] Packaging ---'
rm -rf "${DEPLOY_DIR}"
mkdir -p "${DEPLOY_DIR}/${PACKAGE_NAME}/bin"
mkdir -p "${DEPLOY_DIR}/${PACKAGE_NAME}/config"
mkdir -p "${DEPLOY_DIR}/${PACKAGE_NAME}/www"
mkdir -p "${DEPLOY_DIR}/${PACKAGE_NAME}/logs"

# 复制二进制
cp "${BUILD_DIR}/iotgw_gateway" "${DEPLOY_DIR}/${PACKAGE_NAME}/bin/"

# 复制配置
cp -r IotEdgeGateway/config/* "${DEPLOY_DIR}/${PACKAGE_NAME}/config/"

# 复制前端资源
cp -r IotEdgeGateway/www/* "${DEPLOY_DIR}/${PACKAGE_NAME}/www/"

# 自动生成部署配置 (deployment.yaml)
DEPLOY_YAML="${DEPLOY_DIR}/${PACKAGE_NAME}/config/environments/deployment.yaml"
cp IotEdgeGateway/config/environments/development.yaml "${DEPLOY_YAML}"
sed -i 's|config_root: IotEdgeGateway/config|config_root: config|g' "${DEPLOY_YAML}"
sed -i 's|www_root: .*|www_root: www|g' "${DEPLOY_YAML}"
sed -i 's|log_file: .*|log_file: logs/iotgw.log|g' "${DEPLOY_YAML}"

# 创建启动脚本 start.sh
# 使用 EOF (不带引号) 使得 $0 和 pwd 保持原样写入文件
START_SH="${DEPLOY_DIR}/${PACKAGE_NAME}/start.sh"
cat > "${START_SH}" <<'INNER_EOF'
#!/bin/bash
cd "$(dirname "$0")"
mkdir -p logs
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH

ROOT_DIR=$(pwd)
echo "Starting IoT Gateway in $ROOT_DIR"

# 动态替换配置中的 www_root 为绝对路径
# 这是一个临时的 sed 操作，确保 Mongoose 能找到 www 目录
sed -i "s|www_root: .*|www_root: $ROOT_DIR/www|g" config/environments/deployment.yaml

# 启动网关
./bin/iotgw_gateway --yaml-config config/environments/deployment.yaml
INNER_EOF

chmod +x "${START_SH}"

# 压缩
echo '--- Compressing Artifacts ---'
TAR_NAME="iotgw-${GIT_COMMIT}-v${VERSION}.tar.gz"
cd "${DEPLOY_DIR}"
tar -czf "${TAR_NAME}" "${PACKAGE_NAME}"

echo "Build Success!"
echo "Package: ${DEPLOY_DIR}/${TAR_NAME}"
EOF

chmod +x "$SCRIPT_DIR/entrypoint_build.sh"

docker run --rm \
    -v "$PROJECT_ROOT:/workspace" \
    -v "$SCRIPT_DIR/entrypoint_build.sh:/entrypoint_build.sh" \
    "$IMAGE_NAME" bash /entrypoint_build.sh

# 清理临时脚本
rm -f "$SCRIPT_DIR/entrypoint_build.sh"

echo "Done! Artifacts are ready in $PROJECT_ROOT/build/aarch64/release/deploy"
