# API Reference

本文档描述当前网关程序已实现的 HTTP/WebSocket 接口，便于联调与长期维护。

## HTTP API

- 监听地址：默认 `http://0.0.0.0:8000`
- Base path：`/api`
- Content-Type：响应为 `application/json`

### GET /api/health

用途：健康检查。

响应：
- 200

示例：
```json
{"status":"ok"}
```

### GET /api/version

用途：查询网关版本。

响应：
- 200

示例：
```json
{"version":"0.1.0"}
```

说明：当前版本号为服务端固定返回值，后续应改为读取 UpdateManager/VersionController 的实际版本。

### 其他路径

- 未实现的路径统一返回：404 + `Not Found`

## WebSocket

- URL：默认 `ws://0.0.0.0:8000/ws`
- 协议：WebSocket（当前实现为“原样回显”）

### 行为

- 客户端发送任意文本消息后，服务端会将消息内容原样返回（echo）。

### 示例

请求：
```text
hello
```

响应：
```text
hello
```

## MQTT

- 目前仅预留接入位置，尚未在网关进程内启用 MQTT 连接与订阅/发布逻辑。
