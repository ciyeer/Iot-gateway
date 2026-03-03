# Changelog

All notable changes to the IoT Edge Gateway project will be documented in this file.

## [v0.9.1] - 2026-03-01
### Added
- **Packaging**: Enhanced `package_rk3568.sh` with support for `-r` (Release) and `-d` (Debug) modes.
- **Packaging**: Added `-c` / `--clean` flag to package script for one-click workspace cleanup.
- **Project Structure**: Introduced dedicated `deploy/` directory for build artifacts, keeping project root clean.
- **Project Structure**: Added `IotEdgeGateway/www` directory for static web resources, separating them from configuration files.

### Changed
- **Build System**: Updated `build.sh` to map `-c` to full clean (removed redundant `-cleanall`).
- **Configuration**: Updated `development.yaml` to point `www_root` to the new `www/` directory.
- **Git**: Added `deploy/` and `logs/` to `.gitignore` to prevent accidental commits of runtime/build files.

## [v0.9.0] - 2026-02-28
### Added
- **Deployment**: Initial creation of `package_rk3568.sh` for one-key deployment to RK3568 development boards.
- **Deployment**: Auto-generation of `rk3568.yaml` configuration during packaging (sets Log Level based on build type).

### Fixed
- **Compiler**: Fixed unused variable warnings in `control_api.cpp` (`sent`, `BoolStr`).

## [v0.8.0] - 2026-02-27
### Added
- **Documentation**: Created `config/devices/schema.yaml` defining the standard communication protocol (Telemetry/Command) for MCU developers.

### Changed
- **Protocol**: Standardized MQTT topics to `iotgw/dev/cmd/<device_id>` and `iotgw/dev/telemetry/<device_id>`.

## [v0.7.0] - 2026-02-26
### Fixed
- **Web UI**: Fixed "Switch Bounce" issue by implementing **Optimistic UI Updates** in `control_api.cpp`. The gateway now updates the DeviceRegistry immediately upon receiving a control command.
- **Web UI**: Fixed `index.html` switch state synchronization logic.

## [v0.6.0] - 2026-02-25
### Fixed
- **MQTT**: Fixed incorrect topic construction in `ControlAPI` (removed double prefix issue).
- **Network**: Changed default HTTP port from `8080` to `8088` in development config to avoid conflicts with zombie processes.

## [v0.5.0] - 2026-02-24
### Changed
- **Logging**: Moved log files from project root `logs/` to `data/logs/` to reduce clutter.
- **Logging**: Added more verbose debug logs to `MqttClient` for connection diagnosis (subsequently cleaned up).

## [v0.4.0] - 2026-02-23
### Added
- **Web**: Integrated `index.html` with real-time WebSocket/HTTP polling.
- **Web**: Added "System Log" panel to Web UI for real-time debugging.

### Fixed
- **Config**: Fixed `www_root` path resolution issues in `GatewayCore`.

## [v0.3.0] - 2026-02-22
### Added
- **API**: Implemented `/api/control` POST endpoint for device control.
- **API**: Implemented `/api/status` GET endpoint for full device state retrieval.
- **Core**: Added `HandleControlApi` handler to `RestApi`.

## [v0.2.0] - 2026-02-21
### Added
- **Core**: Implemented `DeviceRegistry` for in-memory device state management.
- **MQTT**: Completed `MqttClient` adapter for bi-directional communication with broker.
- **Config**: Added `sensors.yaml` and `actuators.yaml` device definitions.

## [v0.1.0] - 2026-02-20
### Added
- **Init**: Project initialization with CMake build system.
- **Core**: Basic `GatewayCore` application structure.
- **Deps**: Integrated `mongoose` web server library.
