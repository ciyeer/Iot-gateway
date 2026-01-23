# Changelog

## 0.1.11 - 2026-01-22

### Changed
- VS Code CMake Tools 构建目录调整：按 Kit 与 BuildType 分层输出（`build/<kit>/<Debug|Release>`），便于同时维护 x86_64 与 aarch64 两套构建。
- CMake 配置精简：合并 `iotgw_gateway` 的重复 `target_link_libraries()` 调用。

### Removed
- 删除未使用的空目录：`core/media/`。

## 0.1.10 - 2026-01-21
 
### Added
- 恢复并补齐设备协议适配器骨架：`core/device/protocol_adapters/adapter_base.hpp`、`protocol_adapters/zigbee_adapter/`、`protocol_adapters/modbus_adapter/`。
- Modbus 适配器增加 TCP 占位实现：`protocol_adapters/modbus_adapter/modbus_tcp.cpp`。

### Changed
- `DeviceRegistry::Register()` 在设备 topic 变更时同步清理旧映射，避免脏 topic→device 关系残留。
- 扁平化 `core/control` 目录结构：将 `core/control/rule_engine/rule_engine.hpp` 上移为 `core/control/rule_engine.hpp`，并更新相关 include。

### Removed
- 删除未使用的占位模块：`core/data/`。
- 删除未使用的空壳文件：`core/control/scheduler/`、`core/control/rule_engine/rule_parser.cpp`、`core/control/rule_engine/action_executor.cpp`。
- 删除未使用的设备占位代码：`core/device/drivers/`、`core/device/manager/device_discovery.cpp`、`core/device/model/device_capability.hpp`。

## 0.1.9 - 2026-01-20

### Added
- 新增 REST API 路由模块（`services/web_services/api`），集中管理 System/Device/Rule 三类接口处理逻辑。

### Changed
- 网关 HTTP Handler 从 `GatewayCore` 内联实现迁移到 REST API 路由模块，并通过上下文注入设备注册表、规则引擎与 MQTT 客户端。
- HTTP API 支持从配置项 `network.http_api.base_path` 读取路由前缀（默认 `/api`，并兼容旧前缀回退）。
- 构建系统将 Web REST API 实现加入 `iotgw_common` 静态库编译与链接。
- 将网关核心实现从头文件拆分为 `gateway_core.h` + `gateway_core.cpp`，并更新入口 include 与构建源文件列表。

## 0.1.8 - 2026-01-19

### Added
- 新增设备模型与注册表：维护设备列表、topic 映射、在线状态与最近一次上报信息。
- 新增规则引擎最小可用实现：支持阈值条件判断与动作回调执行（`actuator_set`/`log`）。
- Web 服务支持自定义 HTTP Handler，并在网关中接入管理 API：
  - `GET /api/devices`、`GET /api/devices/<id>`
  - `GET /api/rules`、`POST /api/rules/reload`
  - `POST /api/rules/<id>/enable`、`POST /api/rules/<id>/disable`
  - `POST /api/actuators/<id>/set`

### Changed
- 网关启动流程增加设备与规则配置加载：默认读取 `config/devices/*.yaml` 与 `config/rules/*.yaml`。
- MQTT 上报处理增加设备状态更新与规则触发联动：满足条件时自动发布控制消息到 actuator 的命令 topic。
- 构建系统将设备注册表实现加入 `iotgw_common` 静态库链接。

### Fixed
- 修复 rapidyaml(ryml) 弃用接口告警：将 `ConstNodeRef::valid()` 替换为 `invalid()` 判断。

## 0.1.7 - 2026-01-18

### Changed
- 配置结构调整：将 `config/network/` 下的 MQTT/WebSocket 配置合并到 `config/environments/*.yaml`，运行时只需加载单个环境配置文件。
- 配置加载逻辑简化：启动时不再自动加载 `config/network/*.yaml`。
- `ConfigManager` 增加 YAML 合并加载接口（`LoadYamlFileMerge`），支持按顺序覆盖同名键。
- 环境配置清理：移除未生效的 `network.websocket.host/port` 配置项（WebSocket 与 HTTP 复用同端口）。
- MQTT 配置示例完善：环境配置支持通过用户名/密码连接远端 broker。
- VS Code CMake Tools 配置调整：`cmake.sourceDirectory` 使用 `${workspaceFolder}`，并启用打开工作区自动配置。

### Removed
- 删除配置文件：`config/network/websocket.yaml`、`config/network/mqtt-broker.yaml`（已合并到环境配置）。

## 0.1.6 - 2026-01-17

### Changed
- YAML 解析依赖从 yaml-cpp 切换为 rapidyaml（ryml），并通过 CMake FetchContent 获取与构建。
- Web/MQTT 依赖的 mongoose 通过 CMake FetchContent 获取与构建（不再从 `src/vendor` 编译）。

### Removed
- 移除子模块与目录：`src/vendor/yaml-cpp/`。
- 移除 vendor 目录：`src/vendor/`（不再在仓库中内置第三方源码）。

## 0.1.5 - 2026-01-16

### Added
- Web 服务增加静态文件目录支持：默认从 `/etc/iotgw/www` 提供静态站点。

### Changed
- 配置加载方式切换为仅支持 YAML：移除 `--config` 参数与 KV 配置解析逻辑。
- Web 服务默认监听端口调整为 `8080`（HTTP/WS 同端口）。
- 构建系统强制要求内置 `src/vendor/yaml-cpp` 存在（缺失则 CMake 配置阶段直接失败）。
- 更新部署/快速启动说明，对齐 YAML 配置与默认端口。

### Fixed
- 修复 yaml-cpp 在 GCC 6.x 交叉工具链下 `dragonbox.h` 的 `min/max` 编译失败问题。

## 0.1.4 - 2026-01-15

### Added
- 引入 yaml-cpp 源码到 `src/vendor/yaml-cpp/`，用于离线构建场景的 YAML 解析依赖。

### Changed
- 构建系统以“源码直编”方式集成 yaml-cpp（不依赖其自带 CMake），并链接到 `iotgw_common`。

### Removed
- 删除未使用的目录 `IotEdgeGateway/vendor/`。

### Fixed
- 修复多处 C++17 嵌套命名空间写法导致的 C++14 + `-Wpedantic` 编译告警。
- 修复 `return std::move(default_value)` 引发的冗余 move 告警。

## 0.1.3 - 2026-01-14

### Added
- 新增基于 Mongoose 的 MQTT 客户端能力，并接入网关主循环实现 MQTT↔WebSocket 双向转发。
- WebSocket 服务增加连接管理与广播接口，用于将 MQTT 上报推送到浏览器端。

### Changed
- WebSocket 下发控制消息改为强制携带 `topic` 字段，不再支持默认 topic 回退发布。
- 配置项 `mqtt_pub_topic` 不再使用（发布 topic 由 Web 消息指定）。

## 0.1.2 - 2026-01-13

### Changed
- 工程标准切换为 C++14，移除对 C++17 标准库头（filesystem/optional/string_view）的依赖，适配 GCC 6.3.1 交叉工具链。
- Gateway/Config/Update 模块接口调整为纯 C++14 形态（`std::string` + `bool`/out 参数），并用 POSIX 文件/目录操作替代 filesystem。

### Removed
- 移除 C++17 兼容层 `std_compat.hpp`（代码已不再引用）。
- 删除未被引用的兼容头 `fs_compat.hpp`。

### Fixed
- 修复因工具链缺失 `<filesystem>`/`<optional>` 标准头导致的交叉编译失败。

## 0.1.1 - 2026-01-12

### Changed
- 更新 API 文档，从占位内容调整为“已实现接口”说明，并补充请求/响应示例与 WebSocket 行为说明。
- 更新部署文档，对齐当前网关默认监听端口与路径（HTTP/WS 同端口，WS 路径 `/ws`），并补充交叉编译产物运行提示。

## 0.1.0 - 2026-01-11

### Added
- 新增网关可执行程序目标 `iotgw_gateway`，实现基础启动流程与主循环。
- 新增基于 Mongoose 的 HTTP/WebSocket 服务能力：
  - WebSocket 路径默认 `/ws`
  - 健康检查接口 `GET /api/health`
  - 版本查询接口 `GET /api/version`
- 新增 C++17 兼容层 `std_compat.hpp`，用于兼容老版本工具链（如 GCC 6.x 的 experimental filesystem/optional/string_view）。
- 新增版本/更新管理相关能力（version controller + update manager 基础框架）。

### Changed
- 构建系统启用 C 语言编译能力（`LANGUAGES CXX C`），以便编译 vendor 的 C 源码。
- vendor 的 mongoose 源码调整到 `src/vendor/mongoose/` 并参与 `iotgw_gateway` 构建。
- 架构文档中移除 `presentation` 模块描述（该模块已删除）。

### Removed
- 删除 `src/presentation` 占位模块（Qt/Web UI 空桩）。

### Fixed
- 修复在部分工具链中 `std::optional` 不支持 `has_value()` 导致的编译失败问题（改用可转换为 bool 的写法）。
- 修复 mongoose 源码路径/语言类型导致的链接失败问题，确保交叉编译链路可正常链接。
