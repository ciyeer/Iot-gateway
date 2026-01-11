# API Reference (Placeholder)

This repository contains Web UI and service code. Define concrete endpoints and message formats here once the gateway executable is wired.

## HTTP API (Default)
- Base path: `/api`
- Listen: `0.0.0.0:8080`

Suggested endpoints (to implement/confirm):
- `GET /api/health`
- `GET /api/version`
- `GET /api/devices`
- `GET /api/devices/{id}`
- `POST /api/devices/{id}/commands`

## WebSocket (Default)
- Path: `/ws`
- Listen: `0.0.0.0:9002`

Suggested message envelopes:
- `{ "type": "telemetry", "payload": {...} }`
- `{ "type": "event", "payload": {...} }`
- `{ "type": "command_result", "payload": {...} }`

## MQTT (If Enabled)
- Broker: `127.0.0.1:1883`
- Topic prefix: `iotgw/`

Suggested topics:
- `iotgw/telemetry`
- `iotgw/status`
- `iotgw/commands`
