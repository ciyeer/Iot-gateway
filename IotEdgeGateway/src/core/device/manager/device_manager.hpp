#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/device/model/device_entity.hpp"

namespace iotgw {
namespace core {
namespace device {
namespace manager {

class DeviceRegistry {
public:
  bool Register(model::DeviceEntity device);
  bool Has(const std::string& id) const;

  bool Get(const std::string& id, model::DeviceEntity& out) const;
  std::vector<model::DeviceEntity> List() const;

  bool UpdateFromTelemetryTopic(const std::string& topic, const std::string& payload,
                                std::int64_t now_ms, std::string& out_device_id);
  bool UpsertMqttDeviceFromTopic(const std::string& topic, const std::string& payload,
                                 std::int64_t now_ms, std::string& out_device_id);

  bool GetCommandTopic(const std::string& device_id, std::string& out_topic) const;
  bool GetTelemetryTopic(const std::string& device_id, std::string& out_topic) const;

  std::string ToJsonList() const;
  bool ToJsonOne(const std::string& id, std::string& out_json) const;

private:
  static std::string JsonEscape(const std::string& s);
  static std::string JsonQuote(const std::string& s);
  static std::string Bool(bool v);
  static std::string Number(std::int64_t v);

  std::string DeviceToJson(const model::DeviceEntity& d) const;

private:
  std::unordered_map<std::string, model::DeviceEntity> by_id_;
  std::unordered_map<std::string, std::string> telemetry_topic_to_id_;
  std::unordered_map<std::string, std::string> command_topic_to_id_;
};

}  // namespace manager
}  // namespace device
}  // namespace core
}  // namespace iotgw
