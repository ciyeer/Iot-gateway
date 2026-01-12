# Hardware Setup (Template)

This project appears to target an IoT edge gateway. Fill in the sections that match your deployment.

## Network
- Ethernet/Wi-Fi connectivity
- IP configuration (static/DHCP)
- Required open ports (default):
  - HTTP API: 8000
  - WebSocket: 8000 (path: `/ws`)
  - MQTT broker: 1883 (if enabled)

## Serial Devices (Examples)
- Modbus RTU: `/dev/ttyUSB0`
- Zigbee coordinator: `/dev/ttyUSB1`

## Storage Layout
Recommended directories (production):
- Data: `/var/lib/iotgw`
- Logs: `/var/log/iotgw`

说明：网关默认日志文件路径由配置决定；若未配置，示例默认值为 `logs/iotgw.log`（相对路径将落在进程工作目录下）。

## Runtime
- 交叉编译产物应在目标架构（如 RK3568/aarch64 Linux）运行。

## Permissions
Make sure the service user can:
- Read/write data directory
- Write log directory
- Access serial devices (`dialout` group on many distros)
