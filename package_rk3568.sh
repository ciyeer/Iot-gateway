#!/bin/bash

# 默认参数
BUILD_TYPE="release"
DO_CLEAN=false

# 帮助函数
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -d, --debug     Package Debug build"
    echo "  -r, --release   Package Release build (Default)"
    echo "  -c, --clean     Clean deploy directory and exit"
    echo "  -h, --help      Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0              # Same as -r"
    echo "  $0 -r           # Packages release version"
    echo "  $0 -d           # Packages debug version"
    echo "  $0 -c           # Clean deploy directory"
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="release"
            shift
            ;;
        -c|--clean)
            DO_CLEAN=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "Error: Unknown option $1"
            show_help
            exit 1
            ;;
    esac
done

# 设置目录
PROJECT_ROOT="/Users/ciyeer/iwork/Iot-gateway"
BUILD_DIR="${PROJECT_ROOT}/build/aarch64/${BUILD_TYPE}"
DEPLOY_ROOT="${PROJECT_ROOT}/deploy"
PACKAGE_DIR="${DEPLOY_ROOT}/iotgw_package"  # 临时打包目录
CONFIG_SRC="${PROJECT_ROOT}/IotEdgeGateway/config"
WWW_SRC="${PROJECT_ROOT}/IotEdgeGateway/www"

# 处理 Clean 逻辑
if [ "$DO_CLEAN" = true ]; then
    echo "Cleaning deploy directory..."
    if [ -d "${DEPLOY_ROOT}" ]; then
        rm -rf "${DEPLOY_ROOT}"
        echo "Removed ${DEPLOY_ROOT}"
    else
        echo "Nothing to clean."
    fi
    exit 0
fi

echo "Packaging for build type: $BUILD_TYPE"

# 检查编译文件是否存在
if [ ! -f "${BUILD_DIR}/iotgw_gateway" ]; then
    echo "Error: Executable not found at ${BUILD_DIR}/iotgw_gateway"
    echo "Please build the project first using:"
    if [ "$BUILD_TYPE" == "debug" ]; then
        echo "  ./build.sh --debug"
    else
        echo "  ./build.sh --release"
    fi
    exit 1
fi

# 创建打包目录
echo "Creating package directory at ${PACKAGE_DIR}..."
rm -rf "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}/bin"
mkdir -p "${PACKAGE_DIR}/config"
# mkdir -p "${PACKAGE_DIR}/scripts"  # 暂时移除空目录，后续有需要再添加

# 1. 拷贝可执行文件
echo "Copying executable from ${BUILD_DIR}..."
cp "${BUILD_DIR}/iotgw_gateway" "${PACKAGE_DIR}/bin/"

# 2. 拷贝配置文件结构
echo "Copying configurations..."
# 拷贝所有配置文件
cp -r "${CONFIG_SRC}/" "${PACKAGE_DIR}/config/"
# 移除 config 目录下的 index.html，因为它已经移动到了 www 目录
rm -f "${PACKAGE_DIR}/config/index.html"
rm -rf "${PACKAGE_DIR}/config/css"
rm -rf "${PACKAGE_DIR}/config/js"
rm -rf "${PACKAGE_DIR}/config/images"

# 3. 准备生产环境配置 (根据 production.yaml 调整)
# 这里我们创建一个专门针对 RK3568 部署的配置文件 rk3568.yaml
LOG_LEVEL="info"
if [ "$BUILD_TYPE" == "debug" ]; then
    LOG_LEVEL="debug"
fi

cat > "${PACKAGE_DIR}/config/environments/rk3568.yaml" <<EOF
name: IotEdgeGateway
env: production

paths:
  # 相对路径，假设程序在 /opt/iotgw 运行
  config_root: config
  data_dir: data
  log_file: /var/log/iotgw/iotgw.log  # 建议在开发板上使用标准系统日志目录
  www_root: www  # 网页文件放在程序同级目录的 www 文件夹下

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
  topic_prefix: "iotgw/dev/" # 保持与开发环境一致的前缀方便调试
EOF

# 4. 拷贝 Web 静态资源 (www_root 内容)
echo "Copying web resources..."
mkdir -p "${PACKAGE_DIR}/www"
# 拷贝 www 目录下的所有内容
cp -r "${WWW_SRC}/" "${PACKAGE_DIR}/www/"

# 5. 创建启动脚本
echo "Creating start script..."
cat > "${PACKAGE_DIR}/start.sh" <<EOF
#!/bin/bash
cd "\$(dirname "\$0")"
export LD_LIBRARY_PATH=./lib:\$LD_LIBRARY_PATH
chmod +x bin/iotgw_gateway
./bin/iotgw_gateway --yaml-config config/environments/rk3568.yaml
EOF
chmod +x "${PACKAGE_DIR}/start.sh"

# 6. 打包
echo "Compressing package..."
PACKAGE_NAME="iotgw_rk3568_${BUILD_TYPE}_deploy.tar.gz"
cd "${DEPLOY_ROOT}"
tar -czf "${PACKAGE_NAME}" -C iotgw_package .

echo "Done!"
echo "--------------------------------------------------------"
echo "Package File: ${DEPLOY_ROOT}/${PACKAGE_NAME}"
echo "Deploy Dir  : ${PACKAGE_DIR}"
echo "--------------------------------------------------------"
