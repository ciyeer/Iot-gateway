# Quick Start (WSL)

## Prerequisites
- CMake >= 3.20 (your environment uses `/usr/local/bin/cmake`)
- Ninja
- A C++ compiler (GCC/Clang)

## Configure & Build
From repository root:

```bash
rm -rf /home/ciyeer/iwork/IotEdgeGateway/build-wsl/Debug
/usr/local/bin/cmake -S /home/ciyeer/iwork/IotEdgeGateway/IotEdgeGateway \
  -B /home/ciyeer/iwork/IotEdgeGateway/build-wsl/Debug \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug
/usr/local/bin/cmake --build /home/ciyeer/iwork/IotEdgeGateway/build-wsl/Debug -j
```

## Artifacts
Currently the project builds a static library target:
- `libiotgw_common.a` under `build-wsl/Debug/`

If you want an executable, you need an `add_executable(...)` target in CMake.
