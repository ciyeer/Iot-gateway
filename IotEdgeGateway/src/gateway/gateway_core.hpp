#pragma once

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "core/common/config/config_manager.hpp"
#include "core/common/logger/logger.hpp"
#include "core/common/utils/json_utils.hpp"
#include "core/common/utils/time_utils.hpp"
#include "core/control/rule_engine/rule_engine.hpp"
#include "core/device/manager/device_manager.hpp"
#include "core/device/protocol_adapters/mqtt_adapter/mqtt_adapter.hpp"
#include "services/system_services/update/update_manager.hpp"
#include "services/web_services/websocket/websocket_server.hpp"

namespace iotgw {
namespace gateway {

struct Args {
  std::string config_yaml;
  std::string log_file = "logs/iotgw.log";
  std::string log_level = "info";

  bool print_version = false;
  bool has_set_version = false;
  std::string set_version;
};

class GatewayCore {
public:
  static int Run(const Args& args) {
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    Args a = args;

    iotgw::core::common::config::ConfigManager cfg;
    if (!a.config_yaml.empty()) {
      if (cfg.LoadYamlFile(a.config_yaml)) {
        std::string lf;
        if (cfg.GetString("paths.log_file", lf) && !lf.empty()) {
          a.log_file = lf;
        }
        std::string ll;
        if (cfg.GetString("logging.level", ll) && !ll.empty()) {
          a.log_level = ll;
        }
      }
    }

    const std::string log_dir = DirName(a.log_file);
    if (!log_dir.empty()) {
      (void)CreateDirectories(log_dir);
    }

    auto sink = std::make_shared<iotgw::core::common::log::FileSink>(a.log_file);
    auto logger = std::make_shared<iotgw::core::common::log::Logger>(sink);

    iotgw::core::common::log::Level lvl{};
    if (ParseLogLevel(a.log_level, lvl)) {
      logger->SetLevel(lvl);
    }

    iotgw::services::system_services::update::UpdateManager update_mgr(
        iotgw::services::system_services::update::UpdateManager::Options{}, logger);

    if (a.has_set_version) {
      if (!update_mgr.SetCurrentVersion(a.set_version)) {
        std::cerr << "failed to set version\n";
        return 2;
      }
    }

    if (a.print_version) {
      const std::string v = update_mgr.GetCurrentVersionOr("unknown");
      std::cout << v << "\n";
      return 0;
    }

    logger->Info("iotgw starting");
    logger->Info(std::string("log_file=") + a.log_file);

    const std::string v = update_mgr.GetCurrentVersionOr("unknown");
    logger->Info(std::string("current_version=") + v);

    const std::string config_root = cfg.GetStringOr("paths.config_root", "config");

    iotgw::services::web_services::websocket::MongooseServer::Options web_opt;
    std::string http_host;
    std::int64_t http_port = 0;
    if (cfg.GetString("network.http_api.host", http_host) && !http_host.empty() &&
        cfg.GetInt64("network.http_api.port", http_port) && http_port > 0 && http_port <= 65535) {
      web_opt.listen_addr =
          std::string("http://") + http_host + ":" + std::to_string(static_cast<int>(http_port));
    } else {
      std::string host2;
      std::int64_t port2 = 0;
      if (cfg.GetString("listen.host", host2) && !host2.empty() && cfg.GetInt64("listen.port", port2) &&
          port2 > 0 && port2 <= 65535) {
        web_opt.listen_addr =
            std::string("http://") + host2 + ":" + std::to_string(static_cast<int>(port2));
      }
    }

    std::string ws_path;
    if (cfg.GetString("network.websocket.path", ws_path) && !ws_path.empty()) {
      web_opt.ws_path = ws_path;
    } else {
      std::string path2;
      if (cfg.GetString("listen.path", path2) && !path2.empty()) {
        web_opt.ws_path = path2;
      }
    }
    iotgw::services::web_services::websocket::MongooseServer web_server(web_opt, logger);
    if (!web_server.Start()) {
      logger->Error("Failed to start web server on " + web_opt.listen_addr);
    }

    iotgw::core::device::manager::DeviceRegistry device_registry;
    iotgw::core::control::rule_engine::RuleEngine rule_engine;

    {
      iotgw::core::common::config::ConfigManager dcfg;
      (void)dcfg.LoadYamlFileMerge(config_root + "/devices/sensors.yaml");
      (void)dcfg.LoadYamlFileMerge(config_root + "/devices/actuators.yaml");

      std::string topic_prefix;
      if (!((cfg.GetString("mqtt.topic_prefix", topic_prefix) && !topic_prefix.empty()) ||
            (cfg.GetString("topics.prefix", topic_prefix) && !topic_prefix.empty()))) {
        topic_prefix.clear();
      }

      LoadDevicesFromConfig(dcfg, topic_prefix, device_registry);
    }

    {
      std::vector<iotgw::core::control::rule_engine::Rule> rules;
      (void)LoadRulesFromFile(config_root + "/rules/automation-rules.yaml", "automation", rules);
      (void)LoadRulesFromFile(config_root + "/rules/alarm-rules.yaml", "alarm", rules);
      rule_engine.Clear();
      rule_engine.AddRules(std::move(rules));
    }

    iotgw::core::device::protocol_adapters::mqtt::MqttClient mqtt_client(web_server.GetMgr(),
                                                                         logger);
    bool mqtt_enabled = false;
    (void)cfg.GetBool("mqtt.enabled", mqtt_enabled);

    std::string mqtt_topic_prefix;
    if (!((cfg.GetString("mqtt.topic_prefix", mqtt_topic_prefix) && !mqtt_topic_prefix.empty()) ||
          (cfg.GetString("topics.prefix", mqtt_topic_prefix) && !mqtt_topic_prefix.empty()))) {
      mqtt_topic_prefix.clear();
    }

    const std::string rules_automation_file = config_root + "/rules/automation-rules.yaml";
    const std::string rules_alarm_file = config_root + "/rules/alarm-rules.yaml";

    web_server.SetHttpHandler([&](struct mg_connection* c, struct mg_http_message* hm) -> bool {
      const std::string uri(hm->uri.buf, hm->uri.len);

      if (mg_strcmp(hm->method, mg_str("GET")) == 0 && uri == "/api/health") {
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"status\":\"ok\"}\n");
        return true;
      }

      if (mg_strcmp(hm->method, mg_str("GET")) == 0 && uri == "/api/version") {
        const std::string body =
            iotgw::core::common::json::Object({{"version", iotgw::core::common::json::Quote(v)}});
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        return true;
      }

      if (mg_strcmp(hm->method, mg_str("GET")) == 0 && uri == "/api/devices") {
        const std::string body = device_registry.ToJsonList();
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        return true;
      }

      if (mg_strcmp(hm->method, mg_str("GET")) == 0 && StartsWith(uri, "/api/devices/")) {
        const std::string id = uri.substr(std::string("/api/devices/").size());
        std::string body;
        if (!id.empty() && device_registry.ToJsonOne(id, body)) {
          mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        } else {
          mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                        "{\"error\":\"device_not_found\"}\n");
        }
        return true;
      }

      if (mg_strcmp(hm->method, mg_str("GET")) == 0 && uri == "/api/rules") {
        const auto& rs = rule_engine.Rules();
        std::string body;
        body.reserve(256 + rs.size() * 128);
        body.push_back('[');
        bool first = true;
        for (const auto& r : rs) {
          if (!first) body.push_back(',');
          first = false;
          body += iotgw::core::common::json::Object({
              {"id", iotgw::core::common::json::Quote(r.id)},
              {"category", iotgw::core::common::json::Quote(r.category)},
              {"enabled", iotgw::core::common::json::Bool(r.enabled)},
              {"sensor_id", iotgw::core::common::json::Quote(r.when.sensor_id)},
              {"op", iotgw::core::common::json::Quote(r.when.op)},
              {"value", iotgw::core::common::json::Number(r.when.value)},
          });
        }
        body.push_back(']');
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        return true;
      }

      if (mg_strcmp(hm->method, mg_str("POST")) == 0 && uri == "/api/rules/reload") {
        std::vector<iotgw::core::control::rule_engine::Rule> rules;
        (void)LoadRulesFromFile(rules_automation_file, "automation", rules);
        (void)LoadRulesFromFile(rules_alarm_file, "alarm", rules);
        rule_engine.Clear();
        rule_engine.AddRules(std::move(rules));
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"ok\":true}\n");
        return true;
      }

      if (mg_strcmp(hm->method, mg_str("POST")) == 0 && StartsWith(uri, "/api/rules/") &&
          (EndsWith(uri, "/enable") || EndsWith(uri, "/disable"))) {
        const bool enable = EndsWith(uri, "/enable");
        const std::string base = std::string("/api/rules/");
        const std::string tail = enable ? std::string("/enable") : std::string("/disable");
        const std::string id = uri.substr(base.size(), uri.size() - base.size() - tail.size());
        const bool ok = !id.empty() && rule_engine.SetEnabled(id, enable);
        const std::string body =
            iotgw::core::common::json::Object({{"ok", iotgw::core::common::json::Bool(ok)}});
        mg_http_reply(c, ok ? 200 : 404, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        return true;
      }

      if (mg_strcmp(hm->method, mg_str("POST")) == 0 && StartsWith(uri, "/api/actuators/") &&
          EndsWith(uri, "/set")) {
        const std::string base = std::string("/api/actuators/");
        const std::string tail = std::string("/set");
        const std::string id = uri.substr(base.size(), uri.size() - base.size() - tail.size());
        if (id.empty()) {
          mg_http_reply(c, 400, "Content-Type: application/json\r\n", "{\"error\":\"missing_id\"}\n");
          return true;
        }

        std::string cmd_topic;
        if (!device_registry.GetCommandTopic(id, cmd_topic)) {
          cmd_topic = mqtt_topic_prefix.empty() ? (std::string("cmd/") + id) : (mqtt_topic_prefix + "cmd/" + id);
        }

        const std::string body_in(hm->body.buf, hm->body.len);
        double num = 0.0;
        bool has_num = mg_json_get_num(mg_str(body_in.c_str()), "$.value", &num);
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

        const bool ok = mqtt_client.IsOpen() && mqtt_client.Publish(cmd_topic, value_out, 0, false);
        const std::string body =
            iotgw::core::common::json::Object({{"ok", iotgw::core::common::json::Bool(ok)}});
        mg_http_reply(c, ok ? 200 : 503, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        return true;
      }

      return false;
    });

    if (mqtt_enabled) {
      iotgw::core::device::protocol_adapters::mqtt::MqttClient::Options mo;
      std::string mqtt_host;
      std::int64_t mqtt_port = 0;
      if ((cfg.GetString("mqtt.broker_host", mqtt_host) && !mqtt_host.empty() &&
           cfg.GetInt64("mqtt.broker_port", mqtt_port) && mqtt_port > 0 && mqtt_port <= 65535) ||
          (cfg.GetString("broker.host", mqtt_host) && !mqtt_host.empty() &&
           cfg.GetInt64("broker.port", mqtt_port) && mqtt_port > 0 && mqtt_port <= 65535)) {
        mo.url = std::string("mqtt://") + mqtt_host + ":" +
                 std::to_string(static_cast<int>(mqtt_port));
      } else {
        mo.url = "mqtt://127.0.0.1:1883";
      }

      if (!(cfg.GetString("mqtt.client_id", mo.client_id) && !mo.client_id.empty())) {
        (void)cfg.GetString("client.client_id", mo.client_id);
      }
      if (!(cfg.GetString("mqtt.username", mo.user) && !mo.user.empty())) {
        (void)cfg.GetString("client.username", mo.user);
      }
      if (!(cfg.GetString("mqtt.password", mo.pass) && !mo.pass.empty())) {
        (void)cfg.GetString("client.password", mo.pass);
      }

      std::int64_t keepalive = 0;
      if (((cfg.GetInt64("mqtt.keepalive_sec", keepalive)) ||
           (cfg.GetInt64("client.keepalive_sec", keepalive))) &&
          keepalive > 0 &&
          keepalive <= 65535) {
        mo.keepalive_sec = static_cast<std::uint16_t>(keepalive);
      }
      bool clean = true;
      if (!cfg.GetBool("mqtt.clean_session", clean)) {
        (void)cfg.GetBool("client.clean_session", clean);
      }
      mo.clean_session = clean;

      (void) mqtt_client.Connect(mo);

      std::string sub_topic;
      if (!(cfg.GetString("mqtt.sub_topic", sub_topic) && !sub_topic.empty())) {
        std::string topic_prefix;
        if (!((cfg.GetString("mqtt.topic_prefix", topic_prefix) && !topic_prefix.empty()) ||
              (cfg.GetString("topics.prefix", topic_prefix) && !topic_prefix.empty()))) {
          topic_prefix.clear();
        }
        if (!topic_prefix.empty()) {
          sub_topic = topic_prefix + "#";
        }
      }
      if (!sub_topic.empty()) (void) mqtt_client.Subscribe(sub_topic, 0);

      mqtt_client.SetMessageHandler([&](const std::string& topic, const std::string& payload) {
        const auto now_ms = iotgw::core::common::time::NowUnixMs();
        std::string device_id;
        (void)device_registry.UpsertMqttDeviceFromTopic(topic, payload, now_ms, device_id);

        double sensor_value = 0.0;
        bool has_value = TryParseSensorValue(payload, sensor_value);
        if (has_value && !device_id.empty()) {
          rule_engine.OnSensorValue(device_id, sensor_value,
                                    [&](const iotgw::core::control::rule_engine::Rule& rule,
                                        const iotgw::core::control::rule_engine::Action& action) {
                                      if (action.type == "actuator_set") {
                                        const std::string act_id = action.actuator_id;
                                        if (act_id.empty()) return;
                                        std::string cmd_topic;
                                        if (!device_registry.GetCommandTopic(act_id, cmd_topic)) {
                                          cmd_topic = mqtt_topic_prefix.empty()
                                                          ? (std::string("cmd/") + act_id)
                                                          : (mqtt_topic_prefix + "cmd/" + act_id);
                                        }
                                        const std::string vout = action.value;
                                        if (!cmd_topic.empty() && mqtt_client.IsOpen()) {
                                          (void)mqtt_client.Publish(cmd_topic, vout, 0, false);
                                        }
                                      } else if (action.type == "log") {
                                        const std::string lvl = ToLower(action.level);
                                        const std::string msg = action.message.empty()
                                                                    ? (std::string("rule_fired: ") + rule.id)
                                                                    : action.message;
                                        if (lvl == "warn" || lvl == "warning") {
                                          logger->Warn(msg);
                                        } else if (lvl == "error") {
                                          logger->Error(msg);
                                        } else if (lvl == "debug") {
                                          logger->Debug(msg);
                                        } else {
                                          logger->Info(msg);
                                        }
                                      }
                                    });
        }

        const std::string frame = iotgw::core::common::json::Object({
            {"type", iotgw::core::common::json::Quote("mqtt_msg")},
            {"topic", iotgw::core::common::json::Quote(topic)},
            {"payload", iotgw::core::common::json::Quote(payload)},
        });
        web_server.BroadcastText(frame);
      });
    }

    web_server.SetWsMessageHandler([&](struct mg_connection* c, const std::string& msg) {
      std::string pub_topic;
      std::string payload;

      char* t = mg_json_get_str(mg_str(msg.c_str()), "$.topic");
      char* p = mg_json_get_str(mg_str(msg.c_str()), "$.payload");
      if (t != nullptr) pub_topic = t;
      if (p != nullptr) payload = p;
      if (t != nullptr) mg_free(t);
      if (p != nullptr) mg_free(p);

      if (pub_topic.empty()) {
        const std::string resp = iotgw::core::common::json::Object({
            {"type", iotgw::core::common::json::Quote("error")},
            {"error", iotgw::core::common::json::Quote("missing_topic")},
        });
        mg_ws_send(c, resp.data(), resp.size(), WEBSOCKET_OP_TEXT);
        return;
      }

      if (mqtt_client.IsOpen()) {
        const bool ok = mqtt_client.Publish(pub_topic, payload, 0, false);
        const std::string resp = iotgw::core::common::json::Object({
            {"type", iotgw::core::common::json::Quote("mqtt_pub_ack")},
            {"ok", iotgw::core::common::json::Bool(ok)},
        });
        mg_ws_send(c, resp.data(), resp.size(), WEBSOCKET_OP_TEXT);
      } else {
        const std::string resp = iotgw::core::common::json::Object({
            {"type", iotgw::core::common::json::Quote("error")},
            {"error", iotgw::core::common::json::Quote("mqtt_not_connected")},
        });
        mg_ws_send(c, resp.data(), resp.size(), WEBSOCKET_OP_TEXT);
      }
    });

    std::int64_t last_heartbeat_ms = 0;

    while (RunningFlag().load()) {
      web_server.Poll(50);

      const auto now = iotgw::core::common::time::NowUnixMs();
      if (last_heartbeat_ms == 0 || now - last_heartbeat_ms >= 10'000) {
        last_heartbeat_ms = now;
        logger->Debug("heartbeat");
        logger->Flush();
      }
      iotgw::core::common::time::SleepMs(50);
    }

    logger->Info("iotgw stopping");
    logger->Flush();
    return 0;
  }

private:
  static std::atomic<bool>& RunningFlag() {
    static std::atomic<bool> running{true};
    return running;
  }

  static void HandleSignal(int) {
    RunningFlag().store(false);
  }

  static std::string DirName(const std::string& p) {
    const auto pos = p.find_last_of('/');
    if (pos == std::string::npos) return std::string();
    if (pos == 0) return std::string("/");
    return p.substr(0, pos);
  }

  static bool CreateDirectories(const std::string& dir) {
    if (dir.empty()) return true;

    std::string path;
    size_t i = 0;
    if (dir[0] == '/') {
      path = "/";
      i = 1;
    }

    while (i <= dir.size()) {
      const auto j = dir.find('/', i);
      const std::string part = (j == std::string::npos) ? dir.substr(i) : dir.substr(i, j - i);
      if (!part.empty()) {
        if (path.size() > 1 && path.back() != '/') path.push_back('/');
        path += part;

        if (::mkdir(path.c_str(), 0755) != 0) {
          if (errno != EEXIST) return false;
        }
      }

      if (j == std::string::npos) break;
      i = j + 1;
    }

    return true;
  }

  static std::string ToLower(std::string s) {
    for (char& c : s) {
      const unsigned char uc = static_cast<unsigned char>(c);
      if (uc >= 'A' && uc <= 'Z') c = static_cast<char>(uc - 'A' + 'a');
    }
    return s;
  }

  static bool ParseLogLevel(const std::string& s, iotgw::core::common::log::Level& out) {
    const std::string t = ToLower(s);
    if (t == "trace") {
      out = iotgw::core::common::log::Level::Trace;
      return true;
    }
    if (t == "debug") {
      out = iotgw::core::common::log::Level::Debug;
      return true;
    }
    if (t == "info") {
      out = iotgw::core::common::log::Level::Info;
      return true;
    }
    if (t == "warn" || t == "warning") {
      out = iotgw::core::common::log::Level::Warn;
      return true;
    }
    if (t == "error") {
      out = iotgw::core::common::log::Level::Error;
      return true;
    }
    if (t == "fatal") {
      out = iotgw::core::common::log::Level::Fatal;
      return true;
    }
    return false;
  }

  static bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
  }

  static bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  static bool TryParseDoubleStrict(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) return false;
    while (end != nullptr && *end != '\0') {
      if (*end != ' ' && *end != '\n' && *end != '\r' && *end != '\t') return false;
      ++end;
    }
    out = v;
    return true;
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

  static bool TryParseSensorValue(const std::string& payload, double& out) {
    if (TryParseDoubleStrict(payload, out)) return true;
    if (payload.empty()) return false;
    double v = 0.0;
    if (mg_json_get_num(mg_str(payload.c_str()), "$.value", &v)) {
      out = v;
      return true;
    }
    return false;
  }

  static bool LoadRulesFromFile(const std::string& file_path, const std::string& category,
                                std::vector<iotgw::core::control::rule_engine::Rule>& out_rules) {
    iotgw::core::common::config::ConfigManager rcfg;
    if (!rcfg.LoadYamlFile(file_path)) return false;

    const std::string key = category + "_rules";
    std::size_t i = 0;
    while (true) {
      const std::string base = key + "[" + std::to_string(i) + "].";
      std::string id;
      if (!(rcfg.GetString(base + "id", id) && !id.empty())) break;

      iotgw::core::control::rule_engine::Rule r;
      r.id = id;
      r.category = category;

      bool enabled = true;
      if (rcfg.GetBool(base + "enabled", enabled)) r.enabled = enabled;

      (void)rcfg.GetString(base + "when.sensor_id", r.when.sensor_id);
      (void)rcfg.GetString(base + "when.op", r.when.op);

      std::string value_s;
      if (rcfg.GetString(base + "when.value", value_s)) {
        double dv = 0.0;
        if (TryParseDoubleStrict(value_s, dv)) r.when.value = dv;
      }

      std::size_t j = 0;
      while (true) {
        const std::string abase = base + "then[" + std::to_string(j) + "].";
        std::string type;
        if (!(rcfg.GetString(abase + "type", type) && !type.empty())) break;
        iotgw::core::control::rule_engine::Action a;
        a.type = type;
        (void)rcfg.GetString(abase + "actuator_id", a.actuator_id);
        (void)rcfg.GetString(abase + "value", a.value);
        (void)rcfg.GetString(abase + "level", a.level);
        (void)rcfg.GetString(abase + "message", a.message);
        r.then.push_back(std::move(a));
        ++j;
      }

      out_rules.push_back(std::move(r));
      ++i;
    }

    return true;
  }

  static void LoadDevicesFromConfig(const iotgw::core::common::config::ConfigManager& cfg,
                                   const std::string& topic_prefix,
                                   iotgw::core::device::manager::DeviceRegistry& out) {
    std::size_t i = 0;
    while (true) {
      const std::string base = std::string("sensors[") + std::to_string(i) + "].";
      std::string id;
      if (!(cfg.GetString(base + "id", id) && !id.empty())) break;
      iotgw::core::device::model::DeviceEntity d;
      d.id = id;
      d.kind = "sensor";
      (void)cfg.GetString(base + "protocol", d.transport);
      d.telemetry_topic = topic_prefix.empty() ? (std::string("telemetry/") + id)
                                               : (topic_prefix + "telemetry/" + id);
      (void)out.Register(std::move(d));
      ++i;
    }

    i = 0;
    while (true) {
      const std::string base = std::string("actuators[") + std::to_string(i) + "].";
      std::string id;
      if (!(cfg.GetString(base + "id", id) && !id.empty())) break;
      iotgw::core::device::model::DeviceEntity d;
      d.id = id;
      d.kind = "actuator";
      (void)cfg.GetString(base + "protocol", d.transport);
      d.command_topic = topic_prefix.empty() ? (std::string("cmd/") + id) : (topic_prefix + "cmd/" + id);
      d.telemetry_topic =
          topic_prefix.empty() ? (std::string("state/") + id) : (topic_prefix + "state/" + id);
      (void)out.Register(std::move(d));
      ++i;
    }
  }
};

}  // namespace gateway
}  // namespace iotgw
