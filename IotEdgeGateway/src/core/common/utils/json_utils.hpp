#pragma once

#include <initializer_list>
#include <string>
#include <utility>

namespace iotgw::core::common::json {

inline std::string Escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (const char c : s) {
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          const char hex[] = "0123456789abcdef";
          out += "\\u00";
          out += hex[(c >> 4) & 0x0f];
          out += hex[c & 0x0f];
        } else {
          out += c;
        }
    }
  }
  return out;
}

inline std::string Quote(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('\"');
  out += Escape(s);
  out.push_back('\"');
  return out;
}

inline std::string Bool(bool v) { return v ? "true" : "false"; }

inline std::string Number(long long v) { return std::to_string(v); }
inline std::string Number(unsigned long long v) { return std::to_string(v); }
inline std::string Number(double v) { return std::to_string(v); }

inline std::string Object(std::initializer_list<std::pair<std::string, std::string>> fields) {
  std::string out;
  out.push_back('{');
  bool first = true;
  for (const auto& kv : fields) {
    if (!first) out.push_back(',');
    first = false;
    out += Quote(kv.first);
    out.push_back(':');
    out += kv.second;
  }
  out.push_back('}');
  return out;
}

}  // namespace iotgw::core::common::json