# API Reference

本文档描述当前网关程序已实现的 HTTP/WebSocket 接口。

## HTTP API

- 监听地址：默认 `http://0.0.0.0:8080` (端口可在 `config/environments/*.yaml` 中配置)
- Base path：`/api`
- Content-Type：响应为 `application/json`

### System

#### `GET /api/health`
健康检查。
- **Response 200**: `{"status":"ok"}`

#### `GET /api/version`
查询网关版本。
- **Response 200**: `{"version":"0.1.0"}`

### Devices

#### `GET /api/devices`
获取所有已注册设备的列表。
- **Response 200**: `[{"id":"node_01", "type":"sensor", ...}, ...]`

#### `GET /api/devices/<device_id>`
获取指定设备的详细信息（包含最新状态）。
- **Response 200**: `{"id":"node_01", "status":"online", "last_seen":1700000000, "data":{...}}`
- **Response 404**: `{"error":"Device not found"}`

#### `POST /api/actuators/<device_id>/set`
向执行器设备下发控制命令。
- **Request**: `{"value": 1}` 或 `{"target_temp": 26}`
- **Response 200**: `{"status":"ok", "message":"Command sent"}`

### Rules

#### `GET /api/rules`
获取当前加载的所有自动化规则。

#### `POST /api/rules/reload`
重新加载规则配置文件。

#### `POST /api/rules/<id>/enable`
启用指定规则。

#### `POST /api/rules/<id>/disable`
禁用指定规则。

## WebSocket

- URL：默认 `ws://0.0.0.0:8080/ws`
- 协议：JSON 消息交互

### 消息格式

所有 WebSocket 消息（发送和接收）均遵循以下 JSON 结构：

```json
{
  "topic": "iotgw/dev/sensors/temp_1",
  "payload": "{\"temperature\": 25.5}"
}
```

- **Client -> Server**: 模拟 MQTT 发布。网关收到消息后，会将其视为从 MQTT 接收到的数据进行处理（触发规则、更新设备状态等）。
- **Server -> Client**: 实时推送。当设备状态更新或 MQTT 收到新消息时，网关会将数据广播给所有连接的 WebSocket 客户端。

## MQTT

- 网关内置 MQTT 客户端，支持连接外部 Broker（如 Mosquitto/EMQX）。
- 配置位置：`config/environments/*.yaml`
- 行为：
  - **订阅**：监听 `mqtt.sub_topic`，收到消息后更新设备状态。
  - **发布**：设备状态变化或规则触发时，向 `mqtt.pub_topic` 发布消息。
