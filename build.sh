#!/bin/bash

# 默认参数
ARCH=""
BUILD_TYPE=""
CLEAN=0
DO_PACKAGE=0

# 解析命令行参数
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -x|--x86) ARCH="x86_64" ;;
        -a|--aarch64|--arm) ARCH="aarch64" ;;
        -d|--debug) BUILD_TYPE="debug" ;;
        -r|--release) BUILD_TYPE="release" ;;
        -c|--clean) CLEAN=1 ;;
        -p|--package) DO_PACKAGE=1 ;;
        -h|--help) 
            echo "Usage: ./build.sh [options]"
            echo "Options:"
            echo "  -a|--arm                Build for aarch64 architecture"
            echo "  -x|--x86                Build for x86_64 architecture"
            echo "  -d|--debug              Build with Debug configuration"
            echo "  -r|--release            Build with Release configuration"
            echo "  -p|--package            Create deployment package after build"
            echo "  -c|--clean              Clean directory (scope depends on other args)"
            echo "  -h|--help               Show this help message"
            echo ""
            echo "Clean Scopes:"
            echo "  ./build.sh -c           -> Clean entire build/ directory"
            echo "  ./build.sh -c -a        -> Clean build/aarch64/ directory"
            echo "  ./build.sh -c -r        -> Clean build/*/release/ directories"
            echo "  ./build.sh -c -a -r     -> Clean build/aarch64/release/ directory"
            exit 0
            ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

# 如果指定了清理
if [ "$CLEAN" -eq 1 ]; then
    CLEAN_PATH="build"
    
    # 场景 1: -c (全清)
    if [ -z "$ARCH" ] && [ -z "$BUILD_TYPE" ]; then
        if [ -d "$CLEAN_PATH" ]; then
            echo "Cleaning entire build directory: $CLEAN_PATH"
            rm -rf "$CLEAN_PATH"
        fi
        exit 0
    fi

    # 场景 2: -c -a (清理特定架构下的所有)
    if [ -n "$ARCH" ] && [ -z "$BUILD_TYPE" ]; then
        CLEAN_PATH="${CLEAN_PATH}/${ARCH}"
        if [ -d "$CLEAN_PATH" ]; then
            echo "Cleaning architecture directory: $CLEAN_PATH"
            rm -rf "$CLEAN_PATH"
        fi
        exit 0
    fi

    # 场景 3: -c -r (清理所有架构下的特定类型)
    if [ -z "$ARCH" ] && [ -n "$BUILD_TYPE" ]; then
        echo "Cleaning build type '$BUILD_TYPE' across all architectures..."
        # 遍历 build 下的所有架构目录
        if [ -d "build" ]; then
            for arch_dir in build/*; do
                if [ -d "$arch_dir/$BUILD_TYPE" ]; then
                    echo "  Removing: $arch_dir/$BUILD_TYPE"
                    rm -rf "$arch_dir/$BUILD_TYPE"
                fi
            done
        fi
        exit 0
    fi

    # 场景 4: -c -a -r (清理特定架构下的特定类型)
    if [ -n "$ARCH" ] && [ -n "$BUILD_TYPE" ]; then
        CLEAN_PATH="${CLEAN_PATH}/${ARCH}/${BUILD_TYPE}"
        if [ -d "$CLEAN_PATH" ]; then
            echo "Cleaning specific build directory: $CLEAN_PATH"
            rm -rf "$CLEAN_PATH"
        else
            echo "Directory not found (nothing to clean): $CLEAN_PATH"
        fi
        exit 0
    fi
    
    exit 0
fi

# 设置默认值用于构建
if [ -z "$ARCH" ]; then ARCH="aarch64"; fi
if [ -z "$BUILD_TYPE" ]; then BUILD_TYPE="debug"; fi

# 映射架构名称到 CMAKE_OSX_ARCHITECTURES
if [ "$ARCH" == "aarch64" ]; then
    OSX_ARCH="arm64"
elif [ "$ARCH" == "x86_64" ]; then
    OSX_ARCH="x86_64"
else
    echo "Error: Unsupported architecture '$ARCH'. Supported: aarch64, x86_64"
    exit 1
fi

# 确保构建类型小写化（用于目录命名）
BUILD_TYPE_LOWER=$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')

# 将构建类型首字母大写（用于 CMake 标准参数 Debug/Release）
# 这样用户输入 "debug" 或 "release" 时，传给 CMake 的仍然是 "Debug"/"Release"
if [ "$BUILD_TYPE_LOWER" == "debug" ]; then
    CMAKE_BUILD_TYPE="Debug"
elif [ "$BUILD_TYPE_LOWER" == "release" ]; then
    CMAKE_BUILD_TYPE="Release"
else
    # 对于其他类型（如 RelWithDebInfo），保持原样或仅首字母大写
    CMAKE_BUILD_TYPE="$BUILD_TYPE"
fi

# 定义路径
SOURCE_DIR="IotEdgeGateway"
BUILD_DIR="build/${ARCH}/${BUILD_TYPE_LOWER}"

echo "=============================================================================="
echo "Configuring Build"
echo "Architecture : $ARCH (OSX: $OSX_ARCH)"
echo "Build Type   : $BUILD_TYPE (CMake: $CMAKE_BUILD_TYPE)"
echo "Source Dir   : $SOURCE_DIR"
echo "Build Dir    : $BUILD_DIR"
echo "=============================================================================="

# 配置 CMake
# -S: 源文件目录
# -B: 构建目录
# -G: 生成器 (Ninja)
# -DCMAKE_BUILD_TYPE: 构建类型
# -DCMAKE_OSX_ARCHITECTURES: 目标架构
echo "Running CMake Configure..."
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DCMAKE_OSX_ARCHITECTURES="$OSX_ARCH" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if [ $? -ne 0 ]; then
    echo "CMake Configure failed!"
    exit 1
fi

# 执行构建
echo "Running CMake Build..."
cmake --build "$BUILD_DIR"

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "=============================================================================="
echo "Build Successful!"
echo "Executable: $BUILD_DIR/iotgw_gateway"
echo "=============================================================================="

# ------------------------------------------------------------------------------
# 自动执行打包流程 (集成 package_rk3568.sh 逻辑)
# ------------------------------------------------------------------------------

if [ "$DO_PACKAGE" -eq 1 ]; then

# 设置部署相关目录
DEPLOY_ROOT="${BUILD_DIR}/deploy"
PACKAGE_DIR="${DEPLOY_ROOT}/iotgw_package"
CONFIG_SRC="${SOURCE_DIR}/config"
WWW_SRC="${SOURCE_DIR}/www"

# 准备打包目录
echo "Packaging..."
rm -rf "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}/bin"
mkdir -p "${PACKAGE_DIR}/config"

# 1. 拷贝可执行文件
cp "${BUILD_DIR}/iotgw_gateway" "${PACKAGE_DIR}/bin/"

# 2. 拷贝配置文件 (排除 Web 资源)
cp -r "${CONFIG_SRC}/" "${PACKAGE_DIR}/config/"
rm -f "${PACKAGE_DIR}/config/index.html"
rm -rf "${PACKAGE_DIR}/config/css" "${PACKAGE_DIR}/config/js" "${PACKAGE_DIR}/config/images"

# 3. 生成 rk3568.yaml 配置
LOG_LEVEL="info"
if [ "$BUILD_TYPE_LOWER" == "debug" ]; then
    LOG_LEVEL="debug"
fi

cat > "${PACKAGE_DIR}/config/environments/rk3568.yaml" <<EOF
name: IotEdgeGateway
env: production

paths:
  config_root: config
  data_dir: data
  log_file: /var/log/iotgw/iotgw.log
  www_root: www

logging:
  level: ${LOG_LEVEL}
  file_sink_enabled: true

network:
  http_api:
    host: 0.0.0.0
    port: 8088
    base_path: /api
  websocket:
    path: /ws

mqtt:
  enabled: true
  broker_host: 118.196.46.169
  broker_port: 1883
  client_id: iotgw-rk3568
  username: "zg"
  password: "12345678"
  keepalive_sec: 30
  clean_session: true
  topic_prefix: "iotgw/dev/"
EOF

# 4. 拷贝 Web 静态资源
if [ -d "${WWW_SRC}" ]; then
    mkdir -p "${PACKAGE_DIR}/www"
    cp -r "${WWW_SRC}/" "${PACKAGE_DIR}/www/"
fi

# 5. 生成启动脚本
cat > "${PACKAGE_DIR}/start.sh" <<EOF
#!/bin/bash
cd "\$(dirname "\$0")"
export LD_LIBRARY_PATH=./lib:\$LD_LIBRARY_PATH
chmod +x bin/iotgw_gateway
./bin/iotgw_gateway --yaml-config config/environments/rk3568.yaml
EOF
chmod +x "${PACKAGE_DIR}/start.sh"

# 6. 压缩打包
mkdir -p "${DEPLOY_ROOT}"
PACKAGE_NAME="iotgw_${ARCH}_${BUILD_TYPE_LOWER}.tar.gz"
tar -czf "${DEPLOY_ROOT}/${PACKAGE_NAME}" -C "${DEPLOY_ROOT}" iotgw_package

echo "Package Created: ${DEPLOY_ROOT}/${PACKAGE_NAME}"
echo "=============================================================================="

fi # End of DO_PACKAGE
