#pragma once

#include <cctype>
#include <cstdint>
#include "core/common/utils/std_compat.hpp"
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace iotgw::core::common::config {

class ConfigManager {
public:
  using Map = std::unordered_map<std::string, std::string>;

  bool LoadKeyValueFile(const std::filesystem::path& file_path) {
    std::ifstream ifs(file_path);
    if (!ifs) return false;

    std::string line;
    while (std::getline(ifs, line)) {
      const auto kv = ParseLine(line);
      if (!kv) continue;
      data_[kv->first] = kv->second;
    }
    return true;
  }

  bool Has(std::string_view key) const {
    return data_.find(std::string(key)) != data_.end();
  }

  std::optional<std::string> GetString(std::string_view key) const {
    const auto it = data_.find(std::string(key));
    if (it == data_.end()) return std::nullopt;
    return it->second;
  }

  std::string GetStringOr(std::string_view key, std::string default_value) const {
    const auto v = GetString(key);
    return v ? *v : std::move(default_value);
  }

  std::optional<std::int64_t> GetInt64(std::string_view key) const {
    const auto v = GetString(key);
    if (!v) return std::nullopt;
    return ParseInt64(*v);
  }

  std::int64_t GetInt64Or(std::string_view key, std::int64_t default_value) const {
    const auto v = GetInt64(key);
    return v ? *v : default_value;
  }

  std::optional<bool> GetBool(std::string_view key) const {
    const auto v = GetString(key);
    if (!v) return std::nullopt;
    return ParseBool(*v);
  }

  bool GetBoolOr(std::string_view key, bool default_value) const {
    const auto v = GetBool(key);
    return v ? *v : default_value;
  }

  const Map& Data() const { return data_; }

private:
  static inline void TrimInPlace(std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    s = s.substr(b, e - b);
  }

  static inline std::optional<std::pair<std::string, std::string>> ParseLine(std::string line) {
    const auto hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);

    TrimInPlace(line);
    if (line.empty()) return std::nullopt;

    const auto eq = line.find('=');
    if (eq == std::string::npos) return std::nullopt;

    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);
    TrimInPlace(key);
    TrimInPlace(val);
    if (key.empty()) return std::nullopt;
    return std::make_pair(std::move(key), std::move(val));
  }

  static inline std::optional<std::int64_t> ParseInt64(const std::string& s) {
    if (s.empty()) return std::nullopt;
    std::int64_t sign = 1;
    size_t i = 0;
    if (s[0] == '-') { sign = -1; i = 1; }
    if (i >= s.size()) return std::nullopt;

    std::int64_t v = 0;
    for (; i < s.size(); ++i) {
      const char c = s[i];
      if (c < '0' || c > '9') return std::nullopt;
      v = v * 10 + (c - '0');
    }
    return v * sign;
  }

  static inline std::optional<bool> ParseBool(const std::string& s) {
    std::string t;
    t.reserve(s.size());
    for (char c : s) t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (t == "1" || t == "true" || t == "yes" || t == "on") return true;
    if (t == "0" || t == "false" || t == "no" || t == "off") return false;
    return std::nullopt;
  }

private:
  Map data_;
};

std::vector<std::string> ValidateRequiredKeys(const ConfigManager& cfg,
                                             const std::vector<std::string>& required_keys);

}  // namespace iotgw::core::common::config