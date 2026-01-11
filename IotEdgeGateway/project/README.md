# IotEdgeGateway (Project)

## Build (WSL)
```bash
rm -rf /home/ciyeer/iwork/IotEdgeGateway/build-wsl/Debug
/usr/local/bin/cmake -S /home/ciyeer/iwork/IotEdgeGateway/IotEdgeGateway \
  -B /home/ciyeer/iwork/IotEdgeGateway/build-wsl/Debug \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug
/usr/local/bin/cmake --build /home/ciyeer/iwork/IotEdgeGateway/build-wsl/Debug -j
```

## Output
- Current CMake target builds a static library: `libiotgw_common.a`
- Config templates are under `IotEdgeGateway/config/`
