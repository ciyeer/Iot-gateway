#pragma once

#include <cstdint>
#include <string>

#include "core/device/model/device_status.hpp"

namespace iotgw {
namespace core {
namespace device {
namespace model {

struct DeviceEntity {
  std::string id;
  std::string kind;
  std::string transport;
  std::string telemetry_topic;
  std::string command_topic;
  DeviceStatus status;
};

}  // namespace model
}  // namespace device
}  // namespace core
}  // namespace iotgw

