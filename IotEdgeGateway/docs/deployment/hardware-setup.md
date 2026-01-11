# Hardware Setup (Template)

This project appears to target an IoT edge gateway. Fill in the sections that match your deployment.

## Network
- Ethernet/Wi-Fi connectivity
- IP configuration (static/DHCP)
- Required open ports:
  - HTTP API: 8080 (default)
  - WebSocket: 9002 (default)
  - MQTT broker: 1883 (if enabled)

## Serial Devices (Examples)
- Modbus RTU: `/dev/ttyUSB0`
- Zigbee coordinator: `/dev/ttyUSB1`

## Storage Layout
Recommended directories (production):
- Data: `/var/lib/iotgw`
- Logs: `/var/log/iotgw`

## Permissions
Make sure the service user can:
- Read/write data directory
- Write log directory
- Access serial devices (`dialout` group on many distros)
