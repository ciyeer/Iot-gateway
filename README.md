# Iot-gateway

---

# 🐌 IoT Edge Gateway Platform（蜗牛物联网监控平台）

> 一套基于 **RK3568** 的嵌入式物联网监控系统，集成设备接入、视频流媒体、Web/Qt 可视化控制。
> Designed for **IoT edge gateway** scenarios with unified device management and real-time video streaming.

---

## 📖 项目简介 · Overview

**蜗牛物联网监控平台** 是一个运行在 **RK3568** 平台上的嵌入式 IoT 网关系统，用于统一管理下位机设备（如 LED、舵机、传感器）并实现 Web 与 Qt 可视化控制。

系统支持：

* 多协议接入（Zigbee / MQTT / UART）
* 视频采集与流媒体（mjpg-streamer / GStreamer）
* Web & Qt 客户端双端控制
* 实时状态推送与数据存储
* 适用于智慧农业、环境监测、安防控制等场景。

---

## 🧱 系统架构 · System Architecture

```
┌──────────────────────────────────────────────┐
│               Web / Qt Client                │
│ • 设备控制    • 实时视频    • 数据曲线   • 推送   │
└──────────────────────▲───────────────────────┘
                       │ HTTP / WS / RTSP
                       ▼
┌──────────────────────┴───────────────────────┐
│              RK3568 IoT Gateway              │
│ ============================================ │
│  Web Server:      Mongoose                   │
│  Device Manager:  Core Logic                 │
│  Protocol:        Zigbee / MQTT / UART       │
│  Media Service:   mjpg-streamer / GStreamer  │
│  Data Center:     SQLite                     │
│  System Service:  systemd                    │
└──────────────────────▲───────────────────────┘
                       │ MQTT / Zigbee / UART
                       ▼
┌──────────────────────┴───────────────────────┐
│                  MCU Nodes                   │
│        • LED      • 舵机      • 传感器         │
│        • 心跳上报  • 状态同步                   │
└──────────────────────────────────────────────┘
```

---

## 🔍 功能特性 · Key Features

| 模块                  | 功能                                 |
| ------------------- | ---------------------------------- |
| 🌐 Web Server       | 基于 Mongoose 的 REST + WebSocket 服务  |
| ⚙️ Device Manager   | 设备注册、状态上报、命令分发                     |
| 📡 Protocol Adapter | 支持 Zigbee / MQTT / 串口等多协议          |
| 🎥 Media Service    | MJPG-Streamer + GStreamer 实现视频流与录像 |
| 💾 Data Center      | SQLite 本地存储设备与传感数据                 |
| 🧩 Frontend         | Web + Qt 客户端，功能一致、接口统一             |
| 🛡️ System          | systemd 守护、自启动、日志管理                |

---

## 🧰 技术栈 · Tech Stack

| 分类     | 使用技术                        |
| ------ | --------------------------- |
| 平台     | RK3568 / Embedded Linux     |
| 编译环境 | macOS + Docker (Cross Compile) |
| 通信协议   | Zigbee / MQTT / UART        |
| Web 服务 | Mongoose / WebSocket / REST |
| 视频流媒体  | mjpg-streamer / GStreamer   |
| 数据存储   | SQLite                      |
| 客户端    | Qt6 / HTML5 / JavaScript    |
| 系统管理   | systemd / Shell 脚本          |

---

## 📂 项目结构 · Directory Layout

```
Iot-gateway/
├── IotEdgeGateway/       # 核心源码与配置
│   ├── CMakeLists.txt
│   ├── src/              # C++ 源代码
│   ├── config/           # 配置文件 (yaml)
│   ├── docs/             # 项目文档
│   └── www/              # 静态 Web 资源
├── tools/                # 开发工具与环境
│   └── docker/           # 交叉编译 Docker 环境
├── build.sh              # 统一构建脚本入口
├── LICENSE
└── README.md
```

---

## 🚀 快速启动 · Quick Start

### 1️⃣ 构建

本项目提供了一个便捷的根目录 `build.sh` 统一构建脚本入口，用于统一管理不同架构和配置的构建过程。

> **注意**：如果您在 macOS 上构建 ARM64 版本（用于 RK3568），需要安装 **Docker Desktop** 以使用交叉编译环境。
> 请使用专用脚本：`./tools/docker/run_docker_build.sh`

**基本用法：**

```bash
./build.sh [选项]
```

**参数说明：**

| 短选项 | 长选项 | 说明 | 默认值 |
| :--- | :--- | :--- | :--- |
| `-a <架构>` | `--arch <架构>` | 指定目标 CPU 架构。支持 `aarch64` (ARM64) 或 `x86_64` (Intel/AMD64)。 | `aarch64` |
| `-d` | `--debug` | 以 **Debug** 模式构建（默认）。 | (默认) |
| `-r` | `--release` | 以 **Release** 模式构建。 | - |
| `-c` | `--clean` | 在构建前先清理（删除）对应的构建目录。 | (不清理) |
| - | `--clean-all` | 直接删除整个 `build/` 文件夹（清理所有架构和构建类型）。 | (不清理) |
| `-h` | `--help` | 显示帮助信息。 | - |

**常见示例：**

1.  **默认构建 (ARM64 debug)**
    ```bash
    ./build.sh
    ```
    产物：`build/aarch64/debug/iotgw_gateway`

2.  **构建 Release 版本**
    ```bash
    ./build.sh -r
    # 或
    ./build.sh --release
    ```
    产物：`build/aarch64/release/iotgw_gateway`

3.  **构建 x86_64 版本** (如 macOS 本地测试)
    ```bash
    ./build.sh -a x86_64
    ```

4.  **清理并构建 Release 版本**
    ```bash
    ./build.sh -c -r
    ```

**说明：**
- 第一次配置会通过 CMake FetchContent 拉取并构建第三方依赖（rapidyaml、mongoose）。
- 如果你使用的是 aarch64 交叉编译工具链，生成的二进制需要拷贝到 RK3568（aarch64 Linux）上运行。

### 2️⃣ 运行与验证接口

在目标设备上启动后，默认监听：
- HTTP：`http://<RK3568_IP>:8080/api/health`
- WebSocket：`ws://<RK3568_IP>:8080/ws`

### 3️⃣ MQTT 联调（网关发布/订阅测试）

#### 3.1 配置 MQTT

在配置文件中启用并填写 MQTT Broker 信息：
- `IotEdgeGateway/IotEdgeGateway/config/environments/development.yaml`
  - `mqtt.enabled: true`
  - `mqtt.broker_host` / `mqtt.broker_port`
  - `mqtt.username` / `mqtt.password`（如需要）
  - `mqtt.topic_prefix`（例如：`iotgw/dev/`）

#### 3.2 MQTTX 订阅（看网关发出的消息）

在 MQTTX 连接到同一个 Broker 后订阅：
- 全部：`<topic_prefix>#`（例如 `iotgw/dev/#`）
- 只看命令：`<topic_prefix>cmd/#`（例如 `iotgw/dev/cmd/#`）

#### 3.3 网关发布（2 种方式）

- 方式 A：HTTP 触发发布（用于 actuator 命令）
  - `POST /api/actuators/<actuator_id>/set`
  - 示例：

```bash
curl -sS -X POST "http://127.0.0.1:8080/api/actuators/relay_1/set" \
  -H "Content-Type: application/json" \
  -d '{"value": 1}'
```

  - 典型发布 topic：`<topic_prefix>cmd/<actuator_id>`（例如 `iotgw/dev/cmd/relay_1`）

- 方式 B：WebSocket 直发（发布任意 topic/payload）
  - 连接：`ws://127.0.0.1:8080/ws`
  - 发送：`{"topic":"<topic>","payload":"<payload>"}`

#### 3.4 网关订阅（从 MQTTX 发布，让网关收到）

网关启动后会订阅：
- `mqtt.sub_topic`（若配置）
- 否则默认：`<topic_prefix>#`

用 MQTTX 发布到 `<topic_prefix>` 下任意 topic（例如 `iotgw/dev/sensors/temp_1`），网关日志里会出现：
- `MQTT rx topic=... payload_len=...`
- `MQTT rx payload=...`

更完整的构建/部署说明请参考：`IotEdgeGateway/docs/deployment/`。

---

## 🧩 统一设备模型 · Unified Device Model

> 所有设备数据均以统一 JSON 模型交互：

```json
{
  "device_id": "node_01",
  "type": "sensor",
  "name": "temp_humi",
  "data": {
    "temperature": 26.5,
    "humidity": 61
  },
  "ts": 1700000000
}
```

---

## 🧠 系统特性 · Design Highlights

* 解耦架构：协议层、控制层、展示层完全独立
* 实时性强：WebSocket 推送 + 低延迟视频流
* 易扩展：新增设备仅需修改配置文件
* 高可用：systemd 守护进程 + 日志管理
* 可视化：Web/Qt 客户端双端控制一致

---

## 🔮 未来计划 · Roadmap

* [ ] OTA 升级机制
* [ ] WebRTC 实时音视频
* [ ] Zigbee Mesh 网络管理
* [ ] Prometheus 指标上报
* [ ] Docker 化部署

---

## 🧑‍💻 作者 · Author

**ciyeer**
嵌入式与 Qt 开发者，专注于 IoT 与智能网关系统设计。

> 如果你对 RK3568 / IoT / 嵌入式 Linux 感兴趣，欢迎一起交流。

---

## 📄 许可证 · License

[MIT License](LICENSE)

---

## 🧷 推荐标签（放在 GitHub Topics）

```
rk3568, embedded-linux, iot-gateway, mqtt, zigbee,
mongoose, gstreamer, mpeg-streamer, qt6, webserver, edge-computing
```

---
