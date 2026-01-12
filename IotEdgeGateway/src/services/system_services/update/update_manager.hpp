#pragma once

#include <cctype>
#include <cstdint>
#include "core/common/utils/std_compat.hpp"
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

#include "core/common/logger/logger.hpp"
#include "core/common/utils/time_utils.hpp"

namespace iotgw::services::system_services::update {

struct SemVer {
  std::int64_t major{};
  std::int64_t minor{};
  std::int64_t patch{};
  std::string prerelease;
  std::string build;

  std::string ToString() const {
    std::string out = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    if (!prerelease.empty()) out += "-" + prerelease;
    if (!build.empty()) out += "+" + build;
    return out;
  }
};

std::optional<SemVer> ParseSemVer(std::string_view s);
int CompareSemVer(const SemVer& a, const SemVer& b);

struct StagedUpdate {
  std::string version;
  std::filesystem::path package_path;
  std::string sha256_hex_lower;
  std::int64_t staged_at_unix_ms{};
};

class UpdateManager {
public:
  using VerifySha256Fn = std::function<bool(const std::filesystem::path&, std::string_view expected_hex_lower)>;
  using ApplyPackageFn = std::function<bool(const std::filesystem::path&)>;

  struct Options {
    std::filesystem::path state_dir = "data/update";
    std::filesystem::path current_version_file;
    std::string default_current_version = "0.0.0";
    bool allow_non_semver = false;
  };

  explicit UpdateManager(Options opt, std::shared_ptr<iotgw::core::common::log::Logger> logger = nullptr)
      : opt_(std::move(opt)), logger_(std::move(logger)) {}

  const Options& GetOptions() const { return opt_; }

  std::filesystem::path StateDir() const { return opt_.state_dir; }

  std::filesystem::path CurrentVersionFile() const {
    if (!opt_.current_version_file.empty()) return opt_.current_version_file;
    return StateDir() / "current_version.txt";
  }

  std::filesystem::path StagedMetaFile() const { return StateDir() / "staged.kv"; }

  std::filesystem::path StagingDir() const { return StateDir() / "staging"; }

  std::optional<std::string> GetCurrentVersion() const {
    const auto v = ReadTextFileTrimmed(CurrentVersionFile());
    if (v && !v->empty()) return v;
    if (!opt_.default_current_version.empty()) return opt_.default_current_version;
    return std::nullopt;
  }

  bool SetCurrentVersion(std::string_view version) {
    return WriteTextFileAtomic(CurrentVersionFile(), std::string(TrimCopy(version)));
  }

  bool IsUpdateAvailable(std::string_view candidate_version) const {
    const auto cur = GetCurrentVersion();
    if (!cur) return true;

    const auto a = ParseSemVer(*cur);
    const auto b = ParseSemVer(candidate_version);
    if (a && b) return CompareSemVer(*a, *b) < 0;

    if (opt_.allow_non_semver) return std::string(candidate_version) != *cur;

    return false;
  }

  std::optional<StagedUpdate> GetStaged() const {
    const auto meta = ReadTextFileTrimmed(StagedMetaFile());
    if (!meta || meta->empty()) return std::nullopt;

    StagedUpdate s;
    std::string_view in = *meta;
    while (!in.empty()) {
      const auto nl = in.find('\n');
      std::string_view line = (nl == std::string_view::npos) ? in : in.substr(0, nl);
      if (nl == std::string_view::npos) in = {};
      else in.remove_prefix(nl + 1);

      line = TrimCopy(line);
      if (line.empty()) continue;

      const auto eq = line.find('=');
      if (eq == std::string_view::npos) continue;

      const auto key = TrimCopy(line.substr(0, eq));
      const auto val = TrimCopy(line.substr(eq + 1));

      if (key == "version") s.version = std::string(val);
      else if (key == "package_path") s.package_path = std::filesystem::path(std::string(val));
      else if (key == "sha256") s.sha256_hex_lower = std::string(val);
      else if (key == "staged_at_unix_ms") s.staged_at_unix_ms = ParseInt64Or(val, 0);
    }

    if (s.version.empty() || s.package_path.empty()) return std::nullopt;
    return s;
  }

  bool ClearStaged() {
    std::error_code ec;
    std::filesystem::remove(StagedMetaFile(), ec);
    return !ec;
  }

  bool StageUpdatePackage(const std::filesystem::path& package_path,
                          std::string_view target_version,
                          std::string expected_sha256_hex_lower,
                          VerifySha256Fn verify_sha256) {
    if (!EnsureDir(StateDir())) return false;
    if (!EnsureDir(StagingDir())) return false;

    std::error_code ec;
    if (!std::filesystem::exists(package_path, ec) || ec) return false;
    if (!std::filesystem::is_regular_file(package_path, ec) || ec) return false;

    if (!expected_sha256_hex_lower.empty()) {
      if (!verify_sha256) return false;
      if (!verify_sha256(package_path, expected_sha256_hex_lower)) return false;
    }

    const auto safe_ver = SanitizeFilename(std::string(TrimCopy(target_version)));
    if (safe_ver.empty()) return false;

    const auto staged_pkg = StagingDir() / (std::string("update_") + safe_ver + ".pkg");

    if (!CopyFileAtomic(package_path, staged_pkg)) return false;

    const auto now_ms = iotgw::core::common::time::NowUnixMs();
    std::string meta;
    meta += "version=" + std::string(TrimCopy(target_version)) + "\n";
    meta += "package_path=" + staged_pkg.string() + "\n";
    meta += "sha256=" + expected_sha256_hex_lower + "\n";
    meta += "staged_at_unix_ms=" + std::to_string(now_ms) + "\n";

    if (!WriteTextFileAtomic(StagedMetaFile(), meta)) return false;

    LogInfo("update staged: " + staged_pkg.string());
    return true;
  }

  bool ApplyStagedUpdate(ApplyPackageFn apply_fn) {
    if (!apply_fn) return false;

    const auto staged = GetStaged();
    if (!staged) return false;

    std::error_code ec;
    if (!std::filesystem::exists(staged->package_path, ec) || ec) return false;

    if (!apply_fn(staged->package_path)) return false;

    if (!SetCurrentVersion(staged->version)) return false;

    const auto history_dir = StateDir() / "history";
    if (!EnsureDir(history_dir)) return false;

    const auto ts = std::to_string(staged->staged_at_unix_ms);
    const auto history_meta = history_dir / ("applied_" + ts + ".kv");

    std::string meta;
    meta += "version=" + staged->version + "\n";
    meta += "package_path=" + staged->package_path.string() + "\n";
    meta += "sha256=" + staged->sha256_hex_lower + "\n";
    meta += "applied_at_unix_ms=" + std::to_string(iotgw::core::common::time::NowUnixMs()) + "\n";

    (void)WriteTextFileAtomic(history_meta, meta);
    (void)ClearStaged();

    LogInfo("update applied: " + staged->version);
    return true;
  }

private:
  static std::string_view TrimCopy(std::string_view s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
  }

  static std::optional<std::string> ReadTextFileTrimmed(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    if (!ifs) return std::nullopt;
    std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    const auto t = TrimCopy(s);
    if (t.empty()) return std::nullopt;
    return std::string(t);
  }

  static bool WriteTextFileAtomic(const std::filesystem::path& p, const std::string& content) {
    std::error_code ec;
    const auto dir = p.parent_path();
    if (!dir.empty()) std::filesystem::create_directories(dir, ec);
    if (ec) return false;

    const auto tmp = p.string() + ".tmp";
    {
      std::ofstream ofs(tmp, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!ofs) return false;
      ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
      if (!ofs) return false;
      ofs.flush();
      if (!ofs) return false;
    }
    std::filesystem::rename(tmp, p, ec);
    if (!ec) return true;

    ec.clear();
    std::filesystem::remove(p, ec);
    ec.clear();
    std::filesystem::rename(tmp, p, ec);
    if (ec) {
      ec.clear();
      std::filesystem::remove(tmp, ec);
      return false;
    }
    return true;
  }

  static bool EnsureDir(const std::filesystem::path& d) {
    std::error_code ec;
    std::filesystem::create_directories(d, ec);
    if (ec) return false;
    return true;
  }

  static bool CopyFileAtomic(const std::filesystem::path& from, const std::filesystem::path& to) {
    std::error_code ec;
    const auto dir = to.parent_path();
    if (!dir.empty()) std::filesystem::create_directories(dir, ec);
    if (ec) return false;

    const auto tmp = to.string() + ".tmp";
    ec.clear();
    std::filesystem::copy_file(from, tmp, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) return false;

    ec.clear();
    std::filesystem::rename(tmp, to, ec);
    if (!ec) return true;

    ec.clear();
    std::filesystem::remove(to, ec);
    ec.clear();
    std::filesystem::rename(tmp, to, ec);
    if (ec) {
      ec.clear();
      std::filesystem::remove(tmp, ec);
      return false;
    }
    return true;
  }

  static std::int64_t ParseInt64Or(std::string_view s, std::int64_t def) {
    s = TrimCopy(s);
    if (s.empty()) return def;
    std::int64_t sign = 1;
    size_t i = 0;
    if (s[0] == '-') {
      sign = -1;
      i = 1;
    }
    if (i >= s.size()) return def;
    std::int64_t v = 0;
    for (; i < s.size(); ++i) {
      const char c = s[i];
      if (c < '0' || c > '9') return def;
      v = v * 10 + (c - '0');
    }
    return v * sign;
  }

  static std::string SanitizeFilename(std::string s) {
    for (char& c : s) {
      const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '.' || c == '_' || c == '-';
      if (!ok) c = '_';
    }
    while (!s.empty() && (s.back() == '.' || s.back() == ' ')) s.pop_back();
    return s;
  }

  void LogInfo(const std::string& msg) const {
    if (logger_) logger_->Info(msg);
  }

private:
  Options opt_;
  std::shared_ptr<iotgw::core::common::log::Logger> logger_;
};

}  // namespace iotgw::services::system_services::update