#pragma once

#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace iotgw {
namespace core {
namespace common {
namespace config {

class ConfigManager {
public:
  using Map = std::unordered_map<std::string, std::string>;

  bool LoadKeyValueFile(const std::string& file_path) {
    std::ifstream ifs(file_path.c_str());
    if (!ifs) return false;

    std::string line;
    while (std::getline(ifs, line)) {
      std::string k;
      std::string v;
      if (!ParseLine(line, k, v)) continue;
      data_[std::move(k)] = std::move(v);
    }
    return true;
  }

  bool Has(const std::string& key) const {
    return data_.find(key) != data_.end();
  }

  bool GetString(const std::string& key, std::string& out) const {
    const auto it = data_.find(key);
    if (it == data_.end()) return false;
    out = it->second;
    return true;
  }

  std::string GetStringOr(const std::string& key, std::string default_value) const {
    std::string out;
    if (GetString(key, out)) return out;
    return default_value;
  }

  bool GetInt64(const std::string& key, std::int64_t& out) const {
    std::string s;
    if (!GetString(key, s)) return false;
    return ParseInt64(s, out);
  }

  std::int64_t GetInt64Or(const std::string& key, std::int64_t default_value) const {
    std::int64_t out = 0;
    if (GetInt64(key, out)) return out;
    return default_value;
  }

  bool GetBool(const std::string& key, bool& out) const {
    std::string s;
    if (!GetString(key, s)) return false;
    return ParseBool(s, out);
  }

  bool GetBoolOr(const std::string& key, bool default_value) const {
    bool out = false;
    if (GetBool(key, out)) return out;
    return default_value;
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

  static inline bool ParseLine(std::string line, std::string& key, std::string& val) {
    const auto hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);

    TrimInPlace(line);
    if (line.empty()) return false;

    const auto eq = line.find('=');
    if (eq == std::string::npos) return false;

    key = line.substr(0, eq);
    val = line.substr(eq + 1);
    TrimInPlace(key);
    TrimInPlace(val);
    return !key.empty();
  }

  static inline bool ParseInt64(const std::string& s, std::int64_t& out) {
    if (s.empty()) return false;
    std::int64_t sign = 1;
    size_t i = 0;
    if (s[0] == '-') {
      sign = -1;
      i = 1;
    }
    if (i >= s.size()) return false;

    std::int64_t v = 0;
    for (; i < s.size(); ++i) {
      const char c = s[i];
      if (c < '0' || c > '9') return false;
      v = v * 10 + (c - '0');
    }
    out = v * sign;
    return true;
  }

  static inline bool ParseBool(const std::string& s, bool& out) {
    std::string t;
    t.reserve(s.size());
    for (char c : s) t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (t == "1" || t == "true" || t == "yes" || t == "on") {
      out = true;
      return true;
    }
    if (t == "0" || t == "false" || t == "no" || t == "off") {
      out = false;
      return true;
    }
    return false;
  }

private:
  Map data_;
};

std::vector<std::string> ValidateRequiredKeys(const ConfigManager& cfg,
                                             const std::vector<std::string>& required_keys);

}  // namespace config
}  // namespace common
}  // namespace core
}  // namespace iotgw
