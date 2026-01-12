# Changelog

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