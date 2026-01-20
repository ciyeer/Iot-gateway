#pragma once

#include <string>

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
  static int Run(const Args& args);
};

}  // namespace gateway
}  // namespace iotgw
