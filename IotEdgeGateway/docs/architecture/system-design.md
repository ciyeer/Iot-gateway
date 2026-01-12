# System Design (Overview)

## High-Level Modules
- `core/common`: shared utilities (logging, config helpers, utils)
- `core/device`: device drivers and protocol adapters
- `core/control`: rule engine, scheduler, command processing
- `core/data`: storage, processing, analytics
- `core/media`: image/video processing and streaming
- `services`: system/web services (auth, websocket, REST API, update, watchdog)

## Configuration
Configuration files live under `config/` and are organized by domain:
- `config/environments/*.yaml`
- `config/network/*.yaml`
- `config/devices/.../*.yaml`
- `config/rules/*.yaml`

At the moment, config parsing/loading policy should be defined by the application layer.
