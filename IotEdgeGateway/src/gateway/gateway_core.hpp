#pragma once

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <sys/stat.h>

#include "core/common/config/config_manager.hpp"
#include "core/common/logger/logger.hpp"
#include "core/common/utils/json_utils.hpp"
#include "core/common/utils/time_utils.hpp"
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

    iotgw::core::device::protocol_adapters::mqtt::MqttClient mqtt_client(web_server.GetMgr(),
                                                                         logger);
    bool mqtt_enabled = false;
    (void)cfg.GetBool("mqtt.enabled", mqtt_enabled);
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

      mqtt_client.SetMessageHandler([&web_server](const std::string& topic,
                                                  const std::string& payload) {
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
};

}  // namespace gateway
}  // namespace iotgw
