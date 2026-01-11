#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace iotgw::core::common::net {

struct Endpoint {
  std::string host;
  std::uint16_t port{};
};

inline bool IsValidPort(std::uint32_t port) {
  return port >= 1 && port <= 65535;
}

inline std::string JoinHostPort(std::string_view host, std::uint16_t port) {
  return std::string(host) + ":" + std::to_string(port);
}

inline std::optional<Endpoint> ParseHostPort(std::string_view s) {
  const auto pos = s.rfind(':');
  if (pos == std::string_view::npos || pos == 0 || pos + 1 >= s.size()) return std::nullopt;

  Endpoint ep;
  ep.host = std::string(s.substr(0, pos));

  std::uint32_t port = 0;
  for (size_t i = pos + 1; i < s.size(); ++i) {
    const char c = s[i];
    if (c < '0' || c > '9') return std::nullopt;
    port = port * 10 + static_cast<std::uint32_t>(c - '0');
    if (port > 65535) return std::nullopt;
  }
  if (!IsValidPort(port)) return std::nullopt;
  ep.port = static_cast<std::uint16_t>(port);
  return ep;
}

}  // namespace iotgw::core::common::net