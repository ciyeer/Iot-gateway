#include "core/device/manager/device_manager.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace iotgw {
namespace core {
namespace device {
namespace manager {

bool DeviceRegistry::Register(model::DeviceEntity device) {
  if (device.id.empty()) return false;
  const std::string id = device.id;
  auto it = by_id_.find(id);
  if (it != by_id_.end()) {
    it->second.kind = device.kind;
    it->second.transport = device.transport;
    it->second.telemetry_topic = device.telemetry_topic;
    it->second.command_topic = device.command_topic;
  } else {
    by_id_.emplace(id, std::move(device));
  }

  const auto& d = by_id_.find(id)->second;
  if (!d.telemetry_topic.empty()) telemetry_topic_to_id_[d.telemetry_topic] = id;
  if (!d.command_topic.empty()) command_topic_to_id_[d.command_topic] = id;
  return true;
}

bool DeviceRegistry::Has(const std::string& id) const { return by_id_.find(id) != by_id_.end(); }

bool DeviceRegistry::Get(const std::string& id, model::DeviceEntity& out) const {
  const auto it = by_id_.find(id);
  if (it == by_id_.end()) return false;
  out = it->second;
  return true;
}

std::vector<model::DeviceEntity> DeviceRegistry::List() const {
  std::vector<model::DeviceEntity> out;
  out.reserve(by_id_.size());
  for (const auto& kv : by_id_) out.push_back(kv.second);
  std::sort(out.begin(), out.end(), [](const model::DeviceEntity& a, const model::DeviceEntity& b) {
    return a.id < b.id;
  });
  return out;
}

bool DeviceRegistry::UpdateFromTelemetryTopic(const std::string& topic, const std::string& payload,
                                              std::int64_t now_ms,
                                              std::string& out_device_id) {
  const auto it = telemetry_topic_to_id_.find(topic);
  if (it == telemetry_topic_to_id_.end()) return false;
  const auto dit = by_id_.find(it->second);
  if (dit == by_id_.end()) return false;
  dit->second.status.online = true;
  dit->second.status.last_seen_ms = now_ms;
  dit->second.status.last_payload = payload;
  dit->second.status.last_topic = topic;
  out_device_id = dit->second.id;
  return true;
}

static std::string LastPathSegment(const std::string& topic) {
  if (topic.empty()) return {};
  const auto pos = topic.find_last_of('/');
  if (pos == std::string::npos) return topic;
  if (pos + 1 >= topic.size()) return {};
  return topic.substr(pos + 1);
}

bool DeviceRegistry::UpsertMqttDeviceFromTopic(const std::string& topic, const std::string& payload,
                                               std::int64_t now_ms,
                                               std::string& out_device_id) {
  if (UpdateFromTelemetryTopic(topic, payload, now_ms, out_device_id)) return true;

  const std::string guessed_id = LastPathSegment(topic);
  if (guessed_id.empty()) return false;

  if (!Has(guessed_id)) {
    model::DeviceEntity d;
    d.id = guessed_id;
    d.kind = "unknown";
    d.transport = "mqtt";
    d.telemetry_topic = topic;
    (void)Register(std::move(d));
  } else {
    auto it = by_id_.find(guessed_id);
    if (it != by_id_.end()) {
      if (!it->second.telemetry_topic.empty()) {
        telemetry_topic_to_id_.erase(it->second.telemetry_topic);
      }
      it->second.telemetry_topic = topic;
      telemetry_topic_to_id_[topic] = guessed_id;
    }
  }

  return UpdateFromTelemetryTopic(topic, payload, now_ms, out_device_id);
}

bool DeviceRegistry::GetCommandTopic(const std::string& device_id, std::string& out_topic) const {
  const auto it = by_id_.find(device_id);
  if (it == by_id_.end()) return false;
  if (it->second.command_topic.empty()) return false;
  out_topic = it->second.command_topic;
  return true;
}

bool DeviceRegistry::GetTelemetryTopic(const std::string& device_id, std::string& out_topic) const {
  const auto it = by_id_.find(device_id);
  if (it == by_id_.end()) return false;
  if (it->second.telemetry_topic.empty()) return false;
  out_topic = it->second.telemetry_topic;
  return true;
}

std::string DeviceRegistry::JsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (const char c : s) {
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          const char hex[] = "0123456789abcdef";
          out += "\\u00";
          out += hex[(c >> 4) & 0x0f];
          out += hex[c & 0x0f];
        } else {
          out += c;
        }
    }
  }
  return out;
}

std::string DeviceRegistry::JsonQuote(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('\"');
  out += JsonEscape(s);
  out.push_back('\"');
  return out;
}

std::string DeviceRegistry::Bool(bool v) { return v ? "true" : "false"; }
std::string DeviceRegistry::Number(std::int64_t v) { return std::to_string(v); }

std::string DeviceRegistry::DeviceToJson(const model::DeviceEntity& d) const {
  std::string out;
  out.reserve(256);
  out.push_back('{');
  out += JsonQuote("id");
  out.push_back(':');
  out += JsonQuote(d.id);
  out.push_back(',');
  out += JsonQuote("kind");
  out.push_back(':');
  out += JsonQuote(d.kind);
  out.push_back(',');
  out += JsonQuote("transport");
  out.push_back(':');
  out += JsonQuote(d.transport);
  out.push_back(',');
  out += JsonQuote("telemetry_topic");
  out.push_back(':');
  out += JsonQuote(d.telemetry_topic);
  out.push_back(',');
  out += JsonQuote("command_topic");
  out.push_back(':');
  out += JsonQuote(d.command_topic);
  out.push_back(',');
  out += JsonQuote("status");
  out.push_back(':');
  out.push_back('{');
  out += JsonQuote("online");
  out.push_back(':');
  out += Bool(d.status.online);
  out.push_back(',');
  out += JsonQuote("last_seen_ms");
  out.push_back(':');
  out += Number(d.status.last_seen_ms);
  out.push_back(',');
  out += JsonQuote("last_topic");
  out.push_back(':');
  out += JsonQuote(d.status.last_topic);
  out.push_back(',');
  out += JsonQuote("last_payload");
  out.push_back(':');
  out += JsonQuote(d.status.last_payload);
  out.push_back('}');
  out.push_back('}');
  return out;
}

std::string DeviceRegistry::ToJsonList() const {
  const auto list = List();
  std::string out;
  out.reserve(256 + list.size() * 128);
  out.push_back('[');
  bool first = true;
  for (const auto& d : list) {
    if (!first) out.push_back(',');
    first = false;
    out += DeviceToJson(d);
  }
  out.push_back(']');
  return out;
}

bool DeviceRegistry::ToJsonOne(const std::string& id, std::string& out_json) const {
  model::DeviceEntity d;
  if (!Get(id, d)) return false;
  out_json = DeviceToJson(d);
  return true;
}

}  // namespace manager
}  // namespace device
}  // namespace core
}  // namespace iotgw
