#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace iotgw {
namespace core {
namespace common {
namespace log {

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

  void Log(Level level, const std::string& msg);
  void Log(Level level, const std::string& tag, const std::string& msg);

  void Trace(const std::string& msg);
  void Debug(const std::string& msg);
  void Info(const std::string& msg);
  void Warn(const std::string& msg);
  void Error(const std::string& msg);
  void Fatal(const std::string& msg);

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
  explicit FileSink(std::string file_path);

  void Write(const Event& e) override;
  void Flush() override;

  std::string Path() const;

private:
  std::string FormatLine(const Event& e) const;

private:
  mutable std::mutex mu_;
  std::string file_path_;
};

}  // namespace log
}  // namespace common
}  // namespace core
}  // namespace iotgw
