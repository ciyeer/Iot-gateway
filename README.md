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
┌──────────────────────────────┐
│        Web / Qt Client        │
│  • 设备控制  • 实时视频  • 数据曲线  │
│  • WebSocket 实时推送            │
└───────────────▲─────────────┘
                │ HTTP / WS / RTSP
┌───────────────┴─────────────┐
│       RK3568 IoT 网关        │
│ ============================ │
│  Web Server: mongoose        │
│  Device Manager              │
│  Protocol Adapter (Zigbee/MQTT/UART) │
│  Media Service (mjpg-streamer/gstreamer)      │
│  Data Center (SQLite)                │
│  System Service (systemd)            │
└───────────────▲─────────────┘
                │ MQTT / Zigbee / UART
┌───────────────┴─────────────┐
│         MCU Nodes            │
│  • LED / 舵机 / 传感器        │
│  • 心跳上报 / 状态同步        │
└──────────────────────────────┘
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
| 通信协议   | Zigbee / MQTT / UART        |
| Web 服务 | Mongoose / WebSocket / REST |
| 视频流媒体  | mjpg-streamer / GStreamer   |
| 数据存储   | SQLite                      |
| 客户端    | Qt6 / HTML5 / JavaScript    |
| 系统管理   | systemd / Shell 脚本          |

---

## 📂 项目结构 · Directory Layout

```
IotEdgeGateway/
├── IotEdgeGateway/
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── core/
│   │   ├── services/
│   │   └── gateway/
│   ├── config/
│   │   ├── environments/
│   │   ├── network/
│   │   ├── devices/
│   │   └── rules/
│   ├── docs/
│   │   ├── architecture/
│   │   ├── deployment/
│   │   └── development/
│   ├── build-deploy/
│   │   ├── docker/
│   │   └── deploy/
│   └── docs/changelog.md
├── LICENSE
└── README.md
```

---

## 🚀 快速启动 · Quick Start

### 1️⃣ 构建

从仓库根目录执行：

```bash
rm -rf build/Debug
cmake -S IotEdgeGateway -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug -j
```

产物：
- `build/Debug/iotgw_gateway`

说明：
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
