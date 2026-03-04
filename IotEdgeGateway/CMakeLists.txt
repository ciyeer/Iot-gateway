cmake_minimum_required(VERSION 3.16)

project(IotEdgeGateway VERSION 0.2.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 获取 Git Commit Hash
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(GIT_COMMIT_HASH "unknown")
endif()

# 获取构建时间
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")

# 生成版本头文件
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/version.hpp.in
    ${CMAKE_BINARY_DIR}/generated/version.hpp
)

# 包含生成目录
include_directories(${CMAKE_BINARY_DIR}/generated)

if (NOT DEFINED FETCHCONTENT_BASE_DIR)
  set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "")
endif()
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

include(FetchContent)

set(MG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mongoose
    GIT_REPOSITORY https://gitee.com/mirrors/mongoose.git
    #GIT_REPOSITORY https://github.com/cesanta/mongoose.git
    GIT_TAG 7.20
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
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
find_package(ryml CONFIG QUIET)
if (NOT ryml_FOUND)
  FetchContent_Declare(ryml
      GIT_REPOSITORY https://gitee.com/mirrors/rapidyaml.git
      GIT_TAG v0.10.0
      GIT_SHALLOW TRUE
      GIT_PROGRESS TRUE
  )
  FetchContent_MakeAvailable(ryml)
endif()

add_library(iotgw_common STATIC
    src/core/common/logger/file_logger.cpp
    src/core/common/config/config_validator.cpp
    src/core/device/manager/device_registry.cpp
    src/core/device/protocol_adapters/mqtt_adapter/mqtt_client.cpp
    src/services/web_services/api/device_api.cpp
    src/services/web_services/api/rule_api.cpp
    src/services/web_services/api/system_api.cpp
    src/services/web_services/api/camera_api.cpp
    src/services/web_services/api/control_api.cpp
    src/services/system_services/update/version_controller.cpp
    src/services/system_services/camera/camera_manager.cpp
)

add_executable(iotgw_gateway
    src/gateway/main.cpp
    src/gateway/gateway_core.cpp
)

# 自动 Strip 减小体积 (Release 模式)
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_STRIP)
    add_custom_command(TARGET iotgw_gateway POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:iotgw_gateway>
        COMMENT "Stripping binary to reduce size"
    )
endif()

target_link_libraries(iotgw_gateway
  PRIVATE
      iotgw_common
      mongoose_static
      ryml::ryml
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

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(iotgw_common PRIVATE -Wall -Wextra -Wpedantic)
endif()
