cmake_minimum_required(VERSION 3.20)

project(IotEdgeGateway VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(MG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mongoose
  GIT_REPOSITORY https://github.com/cesanta/mongoose.git
  GIT_TAG 7.20
  GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(mongoose)

add_library(mongoose_static STATIC
  ${mongoose_SOURCE_DIR}/mongoose.c
)
target_include_directories(mongoose_static
  PUBLIC
    ${mongoose_SOURCE_DIR}
)

set(RYML_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ryml
  GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
  GIT_TAG v0.10.0
  GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(ryml)

add_library(iotgw_common STATIC
  src/core/common/logger/file_logger.cpp
  src/core/common/config/config_validator.cpp
  src/core/device/protocol_adapters/mqtt_adapter/mqtt_client.cpp
  src/services/system_services/update/version_controller.cpp
)

add_executable(iotgw_gateway
  src/gateway/main.cpp
)

target_link_libraries(iotgw_gateway
  PRIVATE
    iotgw_common
    mongoose_static
)

target_include_directories(iotgw_common
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(iotgw_common
  PRIVATE
    mongoose_static
    ryml::ryml
)
target_link_libraries(iotgw_gateway
  PRIVATE
    ryml::ryml
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(iotgw_common PRIVATE -Wall -Wextra -Wpedantic)
endif()
