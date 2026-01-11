#include <atomic>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/common/config/config_manager.hpp"
#include "core/common/logger/logger.hpp"
#include "core/common/utils/time_utils.hpp"
#include "services/system_services/update/update_manager.hpp"

namespace {

std::atomic<bool> g_running{true};

void HandleSignal(int) {
  g_running.store(false);
}

std::string ToLower(std::string s) {
  for (char& c : s) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (uc >= 'A' && uc <= 'Z') c = static_cast<char>(uc - 'A' + 'a');
  }
  return s;
}

std::optional<iotgw::core::common::log::Level> ParseLogLevel(std::string_view s) {
  std::string t = ToLower(std::string(s));
  if (t == "trace") return iotgw::core::common::log::Level::Trace;
  if (t == "debug") return iotgw::core::common::log::Level::Debug;
  if (t == "info") return iotgw::core::common::log::Level::Info;
  if (t == "warn" || t == "warning") return iotgw::core::common::log::Level::Warn;
  if (t == "error") return iotgw::core::common::log::Level::Error;
  if (t == "fatal") return iotgw::core::common::log::Level::Fatal;
  return std::nullopt;
}

struct Args {
  std::filesystem::path config_kv;
  std::filesystem::path log_file = "logs/iotgw.log";
  std::string log_level = "info";

  bool print_version = false;
  std::optional<std::string> set_version;
};

Args ParseArgs(int argc, char** argv) {
  Args out;
  for (int i = 1; i < argc; ++i) {
    const std::string_view a = argv[i];

    auto take_value = [&](std::string_view name) -> std::optional<std::string_view> {
      if (i + 1 >= argc) return std::nullopt;
      ++i;
      return std::string_view(argv[i]);
    };

    if (a == "--config") {
      const auto v = take_value(a);
      if (v.has_value()) out.config_kv = std::filesystem::path(std::string(*v));
    } else if (a == "--log-file") {
      const auto v = take_value(a);
      if (v.has_value()) out.log_file = std::filesystem::path(std::string(*v));
    } else if (a == "--log-level") {
      const auto v = take_value(a);
      if (v.has_value()) out.log_level = std::string(*v);
    } else if (a == "--print-version") {
      out.print_version = true;
    } else if (a == "--set-version") {
      const auto v = take_value(a);
      if (v.has_value()) out.set_version = std::string(*v);
    }
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  auto args = ParseArgs(argc, argv);

  iotgw::core::common::config::ConfigManager cfg;
  if (!args.config_kv.empty()) {
    if (cfg.LoadKeyValueFile(args.config_kv)) {
      if (const auto lf = cfg.GetString("log_file"); lf.has_value() && !lf->empty()) {
        args.log_file = *lf;
      }
      if (const auto ll = cfg.GetString("log_level"); ll.has_value() && !ll->empty()) {
        args.log_level = *ll;
      }
    }
  }

  std::error_code ec;
  if (!args.log_file.parent_path().empty()) {
    std::filesystem::create_directories(args.log_file.parent_path(), ec);
  }

  auto sink = std::make_shared<iotgw::core::common::log::FileSink>(args.log_file);
  auto logger = std::make_shared<iotgw::core::common::log::Logger>(sink);

  if (const auto lvl = ParseLogLevel(args.log_level); lvl.has_value()) {
    logger->SetLevel(*lvl);
  }

  iotgw::services::system_services::update::UpdateManager update_mgr(
      iotgw::services::system_services::update::UpdateManager::Options{}, logger);

  if (args.set_version.has_value()) {
    if (!update_mgr.SetCurrentVersion(*args.set_version)) {
      std::cerr << "failed to set version\n";
      return 2;
    }
  }

  if (args.print_version) {
    const auto v = update_mgr.GetCurrentVersion().value_or("unknown");
    std::cout << v << "\n";
    return 0;
  }

  logger->Info("iotgw starting");
  logger->Info(std::string("log_file=") + args.log_file.string());

  const auto v = update_mgr.GetCurrentVersion().value_or("unknown");
  logger->Info(std::string("current_version=") + v);

  std::int64_t last_heartbeat_ms = 0;

  while (g_running.load()) {
    const auto now = iotgw::core::common::time::NowUnixMs();
    if (last_heartbeat_ms == 0 || now - last_heartbeat_ms >= 10'000) {
      last_heartbeat_ms = now;
      logger->Debug("heartbeat");
      logger->Flush();
    }
    iotgw::core::common::time::SleepMs(200);
  }

  logger->Info("iotgw stopping");
  logger->Flush();
  return 0;
}