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

    if [ -z "$ARCH" ] && [ -z "$BUILD_TYPE" ]; then
        if [ -d "$CLEAN_PATH" ]; then
            echo "Cleaning entire build directory: $CLEAN_PATH"
            rm -rf "$CLEAN_PATH"
        fi
        exit 0
    fi

    if [ -n "$ARCH" ] && [ -z "$BUILD_TYPE" ]; then
        CLEAN_PATH="${CLEAN_PATH}/${ARCH}"
        if [ -d "$CLEAN_PATH" ]; then
            echo "Cleaning architecture directory: $CLEAN_PATH"
            rm -rf "$CLEAN_PATH"
        fi
        exit 0
    fi

    if [ -z "$ARCH" ] && [ -n "$BUILD_TYPE" ]; then
        echo "Cleaning build type '$BUILD_TYPE' across all architectures..."
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

# 设置默认值
if [ -z "$ARCH" ]; then ARCH="aarch64"; fi
if [ -z "$BUILD_TYPE" ]; then BUILD_TYPE="debug"; fi

BUILD_TYPE_LOWER=$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')

if [ "$BUILD_TYPE_LOWER" == "debug" ]; then
    CMAKE_BUILD_TYPE="Debug"
elif [ "$BUILD_TYPE_LOWER" == "release" ]; then
    CMAKE_BUILD_TYPE="Release"
else
    CMAKE_BUILD_TYPE="$BUILD_TYPE"
fi

SOURCE_DIR="IotEdgeGateway"
BUILD_DIR="build/${ARCH}/${BUILD_TYPE_LOWER}"

# 自动识别系统
UNAME_S=$(uname -s)
echo "=============================================================================="
echo "Configuring Build"
echo "System       : $UNAME_S"
echo "Architecture : $ARCH"
echo "Build Type   : $BUILD_TYPE (CMake: $CMAKE_BUILD_TYPE)"
echo "Source Dir   : $SOURCE_DIR"
echo "Build Dir    : $BUILD_DIR"
echo "=============================================================================="

# CMake 跨平台配置
CMAKE_EXTRA=""

if [[ "$UNAME_S" == "Linux" ]]; then
    # Linux 平台
    if [[ "$ARCH" == "aarch64" ]]; then
        # 交叉编译 RK3568
        CMAKE_EXTRA="
        -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc
        -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
        -DCMAKE_SYSTEM_NAME=Linux
        -DCMAKE_SYSTEM_PROCESSOR=aarch64
        "
    fi
elif [[ "$UNAME_S" == "Darwin" ]]; then
    # macOS 平台
    if [[ "$ARCH" == "aarch64" ]]; then
        OSX_ARCH="arm64"
    else
        OSX_ARCH="x86_64"
    fi
    CMAKE_EXTRA="-DCMAKE_OSX_ARCHITECTURES=$OSX_ARCH"
fi

# 执行 CMake
echo "Running CMake Configure..."
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    $CMAKE_EXTRA \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if [ $? -ne 0 ]; then
    echo "CMake Configure failed!"
    exit 1
fi

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

# 打包逻辑不变
if [ "$DO_PACKAGE" -eq 1 ]; then
    echo "Warning: Native packaging via build.sh deprecated for cross-compilation."
    echo "Use scripts/docker/run_docker_build.sh for full release."

    DEPLOY_ROOT="${BUILD_DIR}/deploy"
    PACKAGE_DIR="${DEPLOY_ROOT}/iotgw_package"
    CONFIG_SRC="${SOURCE_DIR}/config"
    WWW_SRC="${SOURCE_DIR}/www"

    rm -rf "${PACKAGE_DIR}"
    mkdir -p "${PACKAGE_DIR}/bin"
    mkdir -p "${PACKAGE_DIR}/config"

    cp "${BUILD_DIR}/iotgw_gateway" "${PACKAGE_DIR}/bin/"
    cp -r "${CONFIG_SRC}/" "${PACKAGE_DIR}/config/"

    if [ -d "${WWW_SRC}" ]; then
        mkdir -p "${PACKAGE_DIR}/www"
        cp -r "${WWW_SRC}/" "${PACKAGE_DIR}/www/"
    fi

    cat > "${PACKAGE_DIR}/start.sh" <<'INNER_EOF'
#!/bin/bash
cd "$(dirname "$0")"
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
./bin/iotgw_gateway --yaml-config config/environments/development.yaml
INNER_EOF
    chmod +x "${PACKAGE_DIR}/start.sh"
    echo "Local test package created at: ${PACKAGE_DIR}"
fi
