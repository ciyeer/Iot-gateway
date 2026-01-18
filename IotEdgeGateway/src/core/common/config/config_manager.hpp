#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fstream>

#include <ryml.hpp>
#include <ryml_std.hpp>

namespace iotgw {
namespace core {
namespace common {
namespace config {

class ConfigManager {
public:
  using Map = std::unordered_map<std::string, std::string>;

  bool LoadYamlFile(const std::string& file_path) {
    data_.clear();
    return LoadYamlFileMerge(file_path);
  }

  bool LoadYamlFileMerge(const std::string& file_path) {
    try {
      std::ifstream file(file_path, std::ios::in | std::ios::binary);
      if (!file) return false;

      std::string contents((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
      if (contents.empty()) return false;

      ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(contents));
      const ryml::ConstNodeRef root = tree.rootref();
      if (!root.valid()) return false;
      FlattenYaml(root, std::string());
      return true;
    } catch (...) {
      return false;
    }
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

  static inline std::string ToStdString(c4::csubstr s) {
    return std::string(s.str, s.len);
  }

  void FlattenYaml(const ryml::ConstNodeRef& node, const std::string& prefix) {
    if (!node.valid()) return;

    if (node.has_val()) {
      if (!prefix.empty()) data_[prefix] = ToStdString(node.val());
      return;
    }

    if (node.is_map()) {
      for (ryml::ConstNodeRef child : node.children()) {
        if (!child.has_key()) continue;
        const std::string ks = ToStdString(child.key());
        const std::string next = prefix.empty() ? ks : (prefix + "." + ks);
        FlattenYaml(child, next);
      }
      return;
    }

    if (node.is_seq()) {
      std::size_t i = 0;
      for (ryml::ConstNodeRef child : node.children()) {
        const std::string next = prefix + "[" + std::to_string(i) + "]";
        FlattenYaml(child, next);
        ++i;
      }
      return;
    }
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
