#include <string>

#include "gateway/gateway_core.h"

namespace {

static bool TakeValue(int& i, int argc, char** argv, std::string& out) {
  if (i + 1 >= argc) return false;
  ++i;
  out = argv[i] ? std::string(argv[i]) : std::string();
  return true;
}

iotgw::gateway::Args ParseArgs(int argc, char** argv) {
  iotgw::gateway::Args out;
  out.config_yaml = "config/environments/development.yaml";
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i] ? std::string(argv[i]) : std::string();

    if (a == "--yaml-config") {
      std::string v;
      if (TakeValue(i, argc, argv, v)) out.config_yaml = v;
    } else if (a == "--log-file") {
      std::string v;
      if (TakeValue(i, argc, argv, v)) out.log_file = v;
    } else if (a == "--log-level") {
      std::string v;
      if (TakeValue(i, argc, argv, v)) out.log_level = v;
    } else if (a == "--print-version") {
      out.print_version = true;
    } else if (a == "--set-version") {
      std::string v;
      if (TakeValue(i, argc, argv, v)) {
        out.has_set_version = true;
        out.set_version = v;
      }
    }
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  const auto args = ParseArgs(argc, argv);
  return iotgw::gateway::GatewayCore::Run(args);
}
