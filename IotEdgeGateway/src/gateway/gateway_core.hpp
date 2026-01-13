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
#include "core/common/utils/time_utils.hpp"
#include "services/system_services/update/update_manager.hpp"
#include "services/web_services/websocket/websocket_server.hpp"

namespace iotgw::gateway {

struct Args {
  std::string config_kv;
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
    if (!a.config_kv.empty()) {
      if (cfg.LoadKeyValueFile(a.config_kv)) {
        std::string lf;
        if (cfg.GetString("log_file", lf) && !lf.empty()) {
          a.log_file = lf;
        }
        std::string ll;
        if (cfg.GetString("log_level", ll) && !ll.empty()) {
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
    std::string addr;
    if (cfg.GetString("http_listen", addr) && !addr.empty()) {
      web_opt.listen_addr = addr;
    }
    iotgw::services::web_services::websocket::MongooseServer web_server(web_opt, logger);
    if (!web_server.Start()) {
      logger->Error("Failed to start web server on " + web_opt.listen_addr);
    }

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

}  // namespace iotgw::gateway