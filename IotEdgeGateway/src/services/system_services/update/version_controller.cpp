#include "services/system_services/update/update_manager.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace iotgw::services::system_services::update {

static bool IsDigit(char c) { return c >= '0' && c <= '9'; }

static std::optional<std::int64_t> ParseNonNegativeInt(std::string_view s, size_t& i) {
  if (i >= s.size() || !IsDigit(s[i])) return std::nullopt;
  std::int64_t v = 0;
  while (i < s.size() && IsDigit(s[i])) {
    v = v * 10 + (s[i] - '0');
    ++i;
  }
  return v;
}

static bool ConsumeChar(std::string_view s, size_t& i, char c) {
  if (i < s.size() && s[i] == c) {
    ++i;
    return true;
  }
  return false;
}

static int CompareIdentifiers(std::string_view a, std::string_view b) {
  size_t ia = 0;
  size_t ib = 0;
  while (true) {
    const auto next_a = a.find('.', ia);
    const auto next_b = b.find('.', ib);

    const std::string_view id_a = (next_a == std::string_view::npos) ? a.substr(ia) : a.substr(ia, next_a - ia);
    const std::string_view id_b = (next_b == std::string_view::npos) ? b.substr(ib) : b.substr(ib, next_b - ib);

    const bool a_empty = id_a.empty();
    const bool b_empty = id_b.empty();
    if (a_empty && b_empty) return 0;
    if (a_empty) return -1;
    if (b_empty) return 1;

    const bool a_num = std::all_of(id_a.begin(), id_a.end(), [](char c) { return IsDigit(c); });
    const bool b_num = std::all_of(id_b.begin(), id_b.end(), [](char c) { return IsDigit(c); });

    if (a_num && b_num) {
      const auto va = std::stoll(std::string(id_a));
      const auto vb = std::stoll(std::string(id_b));
      if (va < vb) return -1;
      if (va > vb) return 1;
    } else if (a_num && !b_num) {
      return -1;
    } else if (!a_num && b_num) {
      return 1;
    } else {
      if (id_a < id_b) return -1;
      if (id_a > id_b) return 1;
    }

    if (next_a == std::string_view::npos && next_b == std::string_view::npos) return 0;
    if (next_a == std::string_view::npos) return -1;
    if (next_b == std::string_view::npos) return 1;

    ia = next_a + 1;
    ib = next_b + 1;
  }
}

std::optional<SemVer> ParseSemVer(std::string_view s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  s = s.substr(b, e - b);
  if (s.empty()) return std::nullopt;

  SemVer v;
  size_t i = 0;

  const auto maj = ParseNonNegativeInt(s, i);
  if (!maj.has_value() || !ConsumeChar(s, i, '.')) return std::nullopt;

  const auto min = ParseNonNegativeInt(s, i);
  if (!min.has_value() || !ConsumeChar(s, i, '.')) return std::nullopt;

  const auto pat = ParseNonNegativeInt(s, i);
  if (!pat.has_value()) return std::nullopt;

  v.major = *maj;
  v.minor = *min;
  v.patch = *pat;

  if (i < s.size() && s[i] == '-') {
    ++i;
    const auto start = i;
    while (i < s.size() && s[i] != '+') ++i;
    v.prerelease = std::string(s.substr(start, i - start));
    if (v.prerelease.empty()) return std::nullopt;
  }

  if (i < s.size() && s[i] == '+') {
    ++i;
    const auto start = i;
    while (i < s.size()) ++i;
    v.build = std::string(s.substr(start, i - start));
    if (v.build.empty()) return std::nullopt;
  }

  if (i != s.size()) return std::nullopt;
  return v;
}

int CompareSemVer(const SemVer& a, const SemVer& b) {
  if (a.major != b.major) return a.major < b.major ? -1 : 1;
  if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
  if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;

  const bool a_pre = !a.prerelease.empty();
  const bool b_pre = !b.prerelease.empty();
  if (!a_pre && !b_pre) return 0;
  if (!a_pre && b_pre) return 1;
  if (a_pre && !b_pre) return -1;

  return CompareIdentifiers(a.prerelease, b.prerelease);
}

}  // namespace iotgw::services::system_services::update