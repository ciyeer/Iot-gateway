# Quick Start (WSL)

## Prerequisites
- CMake >= 3.20 (your environment uses `/usr/local/bin/cmake`)
- Ninja
- A C/C++ compiler (GCC/Clang)

## Configure & Build
From repository root:

```bash
ARCH=x86_64        # 或 aarch64
BUILD_TYPE=Debug   # 或 Release
BUILD_DIR=/home/ciyeer/iwork/IotEdgeGateway/build/${ARCH}/${BUILD_TYPE}

rm -rf "${BUILD_DIR}"
/usr/local/bin/cmake -S /home/ciyeer/iwork/IotEdgeGateway/IotEdgeGateway \
  -B "${BUILD_DIR}" \
  -G Ninja -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
/usr/local/bin/cmake --build "${BUILD_DIR}" -j
```

## Artifacts
- 可执行程序：`iotgw_gateway`（位于 `build/<架构>/<Debug|Release>/` 下）
- 静态库：`libiotgw_common.a`（位于 `build/<架构>/<Debug|Release>/` 下）

说明：如果你使用的是 aarch64 交叉编译工具链，生成的 `iotgw_gateway` 需要拷贝到 RK3568（aarch64 Linux）上运行，不能直接在 x86_64 的 WSL 中执行。

## Run (Minimal)

### 1) 打印版本

在目标板上执行：

```bash
./iotgw_gateway --print-version
```

### 2) 启动 HTTP/WebSocket 服务

默认监听：`http://0.0.0.0:8080`

- HTTP：
  - `GET /api/health`
  - `GET /api/version`
- WebSocket：`/ws`

如需覆盖监听地址，可修改 YAML 配置中的 `network.http_api.host` / `network.http_api.port`，或使用 `--yaml-config <yaml-file>` 指定配置文件。
