#include "services/web_services/api/rest_api.hpp"

#include <cmath>
#include <cstdlib>
#include <string>

#include "core/common/utils/json_utils.hpp"

namespace iotgw {
namespace services {
namespace web_services {
namespace api {

namespace {

static std::string ToStdString(const struct mg_str& s) {
  return std::string(s.buf, s.len);
}

static bool IsMethod(const struct mg_http_message* hm, const char* method) {
  return mg_strcmp(hm->method, mg_str(method)) == 0;
}

static bool StartsWith(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static bool EndsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string FormatNumber(double v) {
  const double iv = std::llround(v);
  if (std::fabs(v - iv) < 1e-9) return std::to_string(static_cast<long long>(iv));
  std::string s = std::to_string(v);
  while (!s.empty() && s.back() == '0') s.pop_back();
  if (!s.empty() && s.back() == '.') s.pop_back();
  if (s.empty()) return "0";
  return s;
}

static std::string DefaultCmdTopic(const std::string& mqtt_topic_prefix, const std::string& device_id) {
  if (mqtt_topic_prefix.empty()) return std::string("cmd/") + device_id;
  return mqtt_topic_prefix + "cmd/" + device_id;
}

}  // namespace

bool HandleDeviceApi(struct mg_connection* c, struct mg_http_message* hm, const std::string& rel_path,
                     const ApiContext& ctx) {
  if (c == nullptr || hm == nullptr) return false;
  if (ctx.device_registry == nullptr) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n", "{\"error\":\"device_registry_null\"}\n");
    return true;
  }

  if (IsMethod(hm, "GET") && rel_path == "/devices") {
    const std::string body = ctx.device_registry->ToJsonList();
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", body.c_str());
    return true;
  }

  if (IsMethod(hm, "GET") && StartsWith(rel_path, "/devices/")) {
    const std::string id = rel_path.substr(std::string("/devices/").size());
    std::string body;
    if (!id.empty() && ctx.device_registry->ToJsonOne(id, body)) {
      mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", body.c_str());
    } else {
      mg_http_reply(c, 404, "Content-Type: application/json\r\n", "{\"error\":\"device_not_found\"}\n");
    }
    return true;
  }

  if (IsMethod(hm, "POST") && StartsWith(rel_path, "/actuators/") && EndsWith(rel_path, "/set")) {
    const std::string base = std::string("/actuators/");
    const std::string tail = std::string("/set");
    const std::string id = rel_path.substr(base.size(), rel_path.size() - base.size() - tail.size());
    if (id.empty()) {
      mg_http_reply(c, 400, "Content-Type: application/json\r\n", "{\"error\":\"missing_id\"}\n");
      return true;
    }

    const std::string body_in = ToStdString(hm->body);

    double num = 0.0;
    const bool has_num = mg_json_get_num(mg_str(body_in.c_str()), "$.value", &num);
    std::string value_out;
    if (has_num) {
      value_out = FormatNumber(num);
    } else {
      char* s = mg_json_get_str(mg_str(body_in.c_str()), "$.value");
      if (s != nullptr) value_out = s;
      if (s != nullptr) mg_free(s);
    }

    if (value_out.empty()) {
      mg_http_reply(c, 400, "Content-Type: application/json\r\n", "{\"error\":\"missing_value\"}\n");
      return true;
    }

    std::string cmd_topic;
    if (!ctx.device_registry->GetCommandTopic(id, cmd_topic)) {
      cmd_topic = DefaultCmdTopic(ctx.mqtt_topic_prefix, id);
    }

    bool ok = false;
    if (ctx.mqtt_client != nullptr && ctx.mqtt_client->IsOpen()) {
      ok = ctx.mqtt_client->Publish(cmd_topic, value_out, 0, false);
    }

    const std::string resp =
        iotgw::core::common::json::Object({{"ok", iotgw::core::common::json::Bool(ok)}});
    mg_http_reply(c, ok ? 200 : 503, "Content-Type: application/json\r\n", "%s\n", resp.c_str());
    return true;
  }

  return false;
}

}  // namespace api
}  // namespace web_services
}  // namespace services
}  // namespace iotgw
