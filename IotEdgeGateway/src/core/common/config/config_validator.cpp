#include "core/common/config/config_manager.hpp"

#include <string>
#include <vector>

namespace iotgw::core::common::config {

static std::string MissingKeyMessage(const std::string& key) {
  return std::string("missing config key: ") + key;
}

std::vector<std::string> ValidateRequiredKeys(const ConfigManager& cfg,
                                             const std::vector<std::string>& required_keys) {
  std::vector<std::string> errors;
  errors.reserve(required_keys.size());
  for (const auto& k : required_keys) {
    if (!cfg.Has(k)) errors.push_back(MissingKeyMessage(k));
  }
  return errors;
}

}  // namespace iotgw::core::common::config