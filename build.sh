#!/bin/bash

# 默认参数
ARCH="aarch64"
BUILD_TYPE="debug"
CLEAN=0
CLEAN_ALL=0

# 解析命令行参数
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -a|--arch) ARCH="$2"; shift ;;
        -d|--debug) BUILD_TYPE="debug" ;;
        -r|--release) BUILD_TYPE="release" ;;
        -c|--clean) CLEAN=1 ;;
        -cleanall) CLEAN_ALL=1 ;;
        -h|--help) 
            echo "Usage: ./build.sh --[options]"
            echo "Options:"
            echo "  -a|--arch <arch>        Architecture (aarch64 | x86_64). Default: aarch64"
            echo "  -d|--debug              Build with Debug configuration (Default)"
            echo "  -r|--release            Build with Release configuration"
            echo "  -c|--clean              Clean build directory before building"
            echo "  -h|--help               Show this help message"
            echo "  -cleanall               Clean entire build folder (removes build/)"
            exit 0
            ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

# 如果指定了 --clean-all，则直接删除整个 build 目录并退出
if [ "$CLEAN_ALL" -eq 1 ]; then
    echo "Cleaning entire build directory: build/"
    rm -rf build
    exit 0
fi

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

# 如果指定了清理，则删除构建目录
if [ "$CLEAN" -eq 1 ]; then
    echo "Cleaning build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

# 配置 CMake
# -S: 源文件目录
# -B: 构建目录
# -G: 生成器 (Ninja)
# -DCMAKE_BUILD_TYPE: 构建类型
# -DCMAKE_OSX_ARCHITECTURES: 目标架构
echo "Running CMake Configure..."
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DCMAKE_OSX_ARCHITECTURES="$OSX_ARCH"

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
