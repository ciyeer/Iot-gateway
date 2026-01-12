#pragma once

#include <chrono>
#include <cstdint>
#include "core/common/utils/std_compat.hpp"
#include <memory>
#include <mutex>
#include <string>

namespace iotgw::core::common::log {

enum class Level : std::uint8_t {
  Trace = 0,
  Debug = 1,
  Info  = 2,
  Warn  = 3,
  Error = 4,
  Fatal = 5
};

struct Event {
  Level level{};
  std::chrono::system_clock::time_point ts{};
  std::string message;
  std::string tag;
};

class Sink {
public:
  virtual ~Sink() = default;
  virtual void Write(const Event& e) = 0;
  virtual void Flush() {}
};

class Logger {
public:
  explicit Logger(std::shared_ptr<Sink> sink);

  void SetLevel(Level level);
  Level GetLevel() const;

  void Log(Level level, std::string_view msg);
  void Log(Level level, std::string_view tag, std::string_view msg);

  void Trace(std::string_view msg);
  void Debug(std::string_view msg);
  void Info(std::string_view msg);
  void Warn(std::string_view msg);
  void Error(std::string_view msg);
  void Fatal(std::string_view msg);

  void Flush();

private:
  bool ShouldLog(Level level) const;

private:
  mutable std::mutex mu_;
  std::shared_ptr<Sink> sink_;
  Level level_{Level::Info};
};

class FileSink final : public Sink {
public:
  explicit FileSink(std::filesystem::path file_path);

  void Write(const Event& e) override;
  void Flush() override;

  std::filesystem::path Path() const;

private:
  std::string FormatLine(const Event& e) const;

private:
  mutable std::mutex mu_;
  std::filesystem::path file_path_;
};

}  // namespace iotgw::core::common::log