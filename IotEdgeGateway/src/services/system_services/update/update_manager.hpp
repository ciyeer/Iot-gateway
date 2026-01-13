#pragma once

#include <cctype>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <string>
#include <sys/stat.h>
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

bool ParseSemVer(const std::string& s, SemVer& out);
int CompareSemVer(const SemVer& a, const SemVer& b);

struct StagedUpdate {
  std::string version;
  std::string package_path;
  std::string sha256_hex_lower;
  std::int64_t staged_at_unix_ms{};
};

class UpdateManager {
public:
  using VerifySha256Fn = std::function<bool(const std::string& path, const std::string& expected_hex_lower)>;
  using ApplyPackageFn = std::function<bool(const std::string& path)>;

  struct Options {
    std::string state_dir = "data/update";
    std::string current_version_file;
    std::string default_current_version = "0.0.0";
    bool allow_non_semver = false;
  };

  explicit UpdateManager(Options opt, std::shared_ptr<iotgw::core::common::log::Logger> logger = nullptr)
      : opt_(std::move(opt)), logger_(std::move(logger)) {}

  const Options& GetOptions() const { return opt_; }

  std::string StateDir() const { return opt_.state_dir; }

  std::string CurrentVersionFile() const {
    if (!opt_.current_version_file.empty()) return opt_.current_version_file;
    return JoinPath(StateDir(), "current_version.txt");
  }

  std::string StagedMetaFile() const { return JoinPath(StateDir(), "staged.kv"); }

  std::string StagingDir() const { return JoinPath(StateDir(), "staging"); }

  bool GetCurrentVersion(std::string& out) const {
    std::string v;
    if (ReadTextFileTrimmed(CurrentVersionFile(), v) && !v.empty()) {
      out = v;
      return true;
    }
    if (!opt_.default_current_version.empty()) {
      out = opt_.default_current_version;
      return true;
    }
    return false;
  }

  std::string GetCurrentVersionOr(std::string default_value) const {
    std::string out;
    if (GetCurrentVersion(out)) return out;
    return std::move(default_value);
  }

  bool SetCurrentVersion(const std::string& version) {
    return WriteTextFileAtomic(CurrentVersionFile(), TrimCopy(version));
  }

  bool IsUpdateAvailable(const std::string& candidate_version) const {
    std::string cur;
    if (!GetCurrentVersion(cur)) return true;

    SemVer a;
    SemVer b;
    if (ParseSemVer(cur, a) && ParseSemVer(candidate_version, b)) return CompareSemVer(a, b) < 0;

    if (opt_.allow_non_semver) return candidate_version != cur;

    return false;
  }

  bool GetStaged(StagedUpdate& out) const {
    std::string meta;
    if (!ReadTextFileTrimmed(StagedMetaFile(), meta) || meta.empty()) return false;

    StagedUpdate s;
    std::istringstream iss(meta);
    std::string line;
    while (std::getline(iss, line)) {
      line = TrimCopy(line);
      if (line.empty()) continue;

      const auto eq = line.find('=');
      if (eq == std::string::npos) continue;

      const std::string key = TrimCopy(line.substr(0, eq));
      const std::string val = TrimCopy(line.substr(eq + 1));

      if (key == "version") s.version = val;
      else if (key == "package_path") s.package_path = val;
      else if (key == "sha256") s.sha256_hex_lower = val;
      else if (key == "staged_at_unix_ms") s.staged_at_unix_ms = ParseInt64Or(val, 0);
    }

    if (s.version.empty() || s.package_path.empty()) return false;
    out = s;
    return true;
  }

  bool ClearStaged() {
    return RemoveFile(StagedMetaFile());
  }

  bool StageUpdatePackage(const std::string& package_path,
                          const std::string& target_version,
                          std::string expected_sha256_hex_lower,
                          VerifySha256Fn verify_sha256) {
    if (!EnsureDir(StateDir())) return false;
    if (!EnsureDir(StagingDir())) return false;

    if (!IsRegularFile(package_path)) return false;

    if (!expected_sha256_hex_lower.empty()) {
      if (!verify_sha256) return false;
      if (!verify_sha256(package_path, expected_sha256_hex_lower)) return false;
    }

    const auto safe_ver = SanitizeFilename(TrimCopy(target_version));
    if (safe_ver.empty()) return false;

    const auto staged_pkg = JoinPath(StagingDir(), std::string("update_") + safe_ver + ".pkg");

    if (!CopyFileAtomic(package_path, staged_pkg)) return false;

    const auto now_ms = iotgw::core::common::time::NowUnixMs();
    std::string meta;
    meta += "version=" + TrimCopy(target_version) + "\n";
    meta += "package_path=" + staged_pkg + "\n";
    meta += "sha256=" + expected_sha256_hex_lower + "\n";
    meta += "staged_at_unix_ms=" + std::to_string(now_ms) + "\n";

    if (!WriteTextFileAtomic(StagedMetaFile(), meta)) return false;

    LogInfo("update staged: " + staged_pkg);
    return true;
  }

  bool ApplyStagedUpdate(ApplyPackageFn apply_fn) {
    if (!apply_fn) return false;

    StagedUpdate staged;
    if (!GetStaged(staged)) return false;

    if (!IsRegularFile(staged.package_path)) return false;

    if (!apply_fn(staged.package_path)) return false;

    if (!SetCurrentVersion(staged.version)) return false;

    const auto history_dir = JoinPath(StateDir(), "history");
    if (!EnsureDir(history_dir)) return false;

    const auto ts = std::to_string(staged.staged_at_unix_ms);
    const auto history_meta = JoinPath(history_dir, "applied_" + ts + ".kv");

    std::string meta;
    meta += "version=" + staged.version + "\n";
    meta += "package_path=" + staged.package_path + "\n";
    meta += "sha256=" + staged.sha256_hex_lower + "\n";
    meta += "applied_at_unix_ms=" + std::to_string(iotgw::core::common::time::NowUnixMs()) + "\n";

    (void)WriteTextFileAtomic(history_meta, meta);
    (void)ClearStaged();

    LogInfo("update applied: " + staged.version);
    return true;
  }

private:
  static std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (!a.empty() && a.back() == '/') return a + b;
    return a + "/" + b;
  }

  static std::string DirName(const std::string& p) {
    const auto pos = p.find_last_of('/');
    if (pos == std::string::npos) return std::string();
    if (pos == 0) return std::string("/");
    return p.substr(0, pos);
  }

  static bool CreateDirectories(const std::string& dir) {
    if (dir.empty()) return true;

    std::string path;
    size_t i = 0;
    if (dir[0] == '/') {
      path = "/";
      i = 1;
    }

    while (i <= dir.size()) {
      const auto j = dir.find('/', i);
      const std::string part = (j == std::string::npos) ? dir.substr(i) : dir.substr(i, j - i);
      if (!part.empty()) {
        if (path.size() > 1 && path.back() != '/') path.push_back('/');
        path += part;

        if (::mkdir(path.c_str(), 0755) != 0) {
          if (errno != EEXIST) {
            struct stat st;
            if (::stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return false;
          }
        }
      }

      if (j == std::string::npos) break;
      i = j + 1;
    }

    return true;
  }

  static bool EnsureDir(const std::string& d) {
    return CreateDirectories(d);
  }

  static bool RemoveFile(const std::string& p) {
    if (::std::remove(p.c_str()) == 0) return true;
    return errno == ENOENT;
  }

  static bool IsRegularFile(const std::string& p) {
    struct stat st;
    if (::stat(p.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
  }

  static std::string TrimCopy(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
  }

  static bool ReadTextFileTrimmed(const std::string& p, std::string& out) {
    std::ifstream ifs(p.c_str(), std::ios::in | std::ios::binary);
    if (!ifs) return false;
    std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    s = TrimCopy(s);
    if (s.empty()) return false;
    out = s;
    return true;
  }

  static bool WriteTextFileAtomic(const std::string& p, const std::string& content) {
    const auto dir = DirName(p);
    if (!dir.empty() && !CreateDirectories(dir)) return false;

    const std::string tmp = p + ".tmp";
    {
      std::ofstream ofs(tmp.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
      if (!ofs) return false;
      ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
      if (!ofs) return false;
      ofs.flush();
      if (!ofs) return false;
    }

    if (::std::rename(tmp.c_str(), p.c_str()) == 0) return true;

    (void)RemoveFile(p);
    if (::std::rename(tmp.c_str(), p.c_str()) == 0) return true;

    (void)RemoveFile(tmp);
    return false;
  }

  static bool CopyFileAtomic(const std::string& from, const std::string& to) {
    const auto dir = DirName(to);
    if (!dir.empty() && !CreateDirectories(dir)) return false;

    std::ifstream ifs(from.c_str(), std::ios::in | std::ios::binary);
    if (!ifs) return false;

    const std::string tmp = to + ".tmp";
    std::ofstream ofs(tmp.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs) return false;

    char buf[8192];
    while (ifs) {
      ifs.read(buf, sizeof(buf));
      const std::streamsize n = ifs.gcount();
      if (n > 0) ofs.write(buf, n);
    }
    if (!ofs) return false;
    ofs.flush();
    if (!ofs) return false;

    if (::std::rename(tmp.c_str(), to.c_str()) == 0) return true;

    (void)RemoveFile(to);
    if (::std::rename(tmp.c_str(), to.c_str()) == 0) return true;

    (void)RemoveFile(tmp);
    return false;
  }

  static std::int64_t ParseInt64Or(const std::string& in, std::int64_t def) {
    const std::string s = TrimCopy(in);
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