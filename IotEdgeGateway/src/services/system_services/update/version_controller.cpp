#include "services/system_services/update/update_manager.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace iotgw::services::system_services::update {

static bool IsDigit(char c) { return c >= '0' && c <= '9'; }

static std::string TrimCopy(const std::string& in) {
  size_t b = 0;
  while (b < in.size() && std::isspace(static_cast<unsigned char>(in[b]))) ++b;
  size_t e = in.size();
  while (e > b && std::isspace(static_cast<unsigned char>(in[e - 1]))) --e;
  return in.substr(b, e - b);
}

static bool ParseNonNegativeInt(const std::string& s, size_t& i, std::int64_t& out) {
  if (i >= s.size() || !IsDigit(s[i])) return false;
  std::int64_t v = 0;
  while (i < s.size() && IsDigit(s[i])) {
    v = v * 10 + (s[i] - '0');
    ++i;
  }
  out = v;
  return true;
}

static bool ConsumeChar(const std::string& s, size_t& i, char c) {
  if (i < s.size() && s[i] == c) {
    ++i;
    return true;
  }
  return false;
}

static bool IsNumeric(const std::string& s) {
  if (s.empty()) return false;
  return std::all_of(s.begin(), s.end(), [](char c) { return IsDigit(c); });
}

static int CompareIdentifiers(const std::string& a, const std::string& b) {
  size_t ia = 0;
  size_t ib = 0;
  while (true) {
    const auto next_a = a.find('.', ia);
    const auto next_b = b.find('.', ib);

    const std::string id_a = (next_a == std::string::npos) ? a.substr(ia) : a.substr(ia, next_a - ia);
    const std::string id_b = (next_b == std::string::npos) ? b.substr(ib) : b.substr(ib, next_b - ib);

    const bool a_empty = id_a.empty();
    const bool b_empty = id_b.empty();
    if (a_empty && b_empty) return 0;
    if (a_empty) return -1;
    if (b_empty) return 1;

    const bool a_num = IsNumeric(id_a);
    const bool b_num = IsNumeric(id_b);

    if (a_num && b_num) {
      const auto va = std::stoll(id_a);
      const auto vb = std::stoll(id_b);
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

    if (next_a == std::string::npos && next_b == std::string::npos) return 0;
    if (next_a == std::string::npos) return -1;
    if (next_b == std::string::npos) return 1;

    ia = next_a + 1;
    ib = next_b + 1;
  }
}

bool ParseSemVer(const std::string& in, SemVer& out) {
  const std::string s = TrimCopy(in);
  if (s.empty()) return false;

  SemVer v;
  size_t i = 0;

  std::int64_t maj = 0;
  if (!ParseNonNegativeInt(s, i, maj) || !ConsumeChar(s, i, '.')) return false;

  std::int64_t min = 0;
  if (!ParseNonNegativeInt(s, i, min) || !ConsumeChar(s, i, '.')) return false;

  std::int64_t pat = 0;
  if (!ParseNonNegativeInt(s, i, pat)) return false;

  v.major = maj;
  v.minor = min;
  v.patch = pat;

  if (i < s.size() && s[i] == '-') {
    ++i;
    const auto start = i;
    while (i < s.size() && s[i] != '+') ++i;
    v.prerelease = s.substr(start, i - start);
    if (v.prerelease.empty()) return false;
  }

  if (i < s.size() && s[i] == '+') {
    ++i;
    const auto start = i;
    while (i < s.size()) ++i;
    v.build = s.substr(start, i - start);
    if (v.build.empty()) return false;
  }

  if (i != s.size()) return false;
  out = v;
  return true;
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