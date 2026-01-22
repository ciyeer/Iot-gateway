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

怎么判断现在生成的是哪种架构
- 看当前选中的 Kit ：如果 Kit 用的是 /usr/bin/g++ 这类本机编译器，就是 x86_64 （WSL）。
- 如果 Kit 用的是 aarch64-linux-gnu-g++ 或某个 aarch64 toolchain file，就是 aarch64 交叉编译产物。

怎么切换成另一种架构
- CMake: Select Kit ：在命令面板里切换 Kit（x86_64 ↔ aarch64）。
  - 如果列表里没有 aarch64：先安装交叉编译器，并运行 CMake: Scan for Kits ；或者用 CMake: Edit User-Local CMake Kits 手动加一个 aarch64 kit（把 C/C++ compiler 指到 aarch64-linux-gnu-gcc/g++ 或配置 toolchain file）。
- 切 Debug/Release：用 CMake: Select Variant （或状态栏的 Debug/Release 入口）。
- 切完 Kit 后建议跑一次 CMake: Delete Cache and Reconfigure ，确保不会复用旧 cache。
让输出目录自动变成 build/aarch64/Debug、build/x86_64/Release

## Permissions
Make sure the service user can:
- Read/write data directory
- Write log directory
- Access serial devices (`dialout` group on many distros)
