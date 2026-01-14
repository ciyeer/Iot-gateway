cmake_minimum_required(VERSION 3.20)

project(IotEdgeGateway VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(iotgw_common STATIC
  src/core/common/logger/file_logger.cpp
  src/core/common/config/config_validator.cpp
  src/core/device/protocol_adapters/mqtt_adapter/mqtt_client.cpp
  src/services/system_services/update/version_controller.cpp
)

add_executable(iotgw_gateway
  src/gateway/main.cpp
  src/vendor/mongoose/mongoose.c
)

target_include_directories(iotgw_gateway
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src/vendor/mongoose
)

target_link_libraries(iotgw_gateway
  PRIVATE
    iotgw_common
)

target_include_directories(iotgw_common
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src/vendor/mongoose
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(iotgw_common PRIVATE -Wall -Wextra -Wpedantic)
endif()
