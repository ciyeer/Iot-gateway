#include "core/common/logger/logger.hpp"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace iotgw::core::common::log {

static const char* ToString(Level level) {
  switch (level) {
    case Level::Trace: return "TRACE";
    case Level::Debug: return "DEBUG";
    case Level::Info:  return "INFO";
    case Level::Warn:  return "WARN";
    case Level::Error: return "ERROR";
    case Level::Fatal: return "FATAL";
    default:           return "UNKNOWN";
  }
}

Logger::Logger(std::shared_ptr<Sink> sink) : sink_(std::move(sink)) {}

void Logger::SetLevel(Level level) {
  std::lock_guard<std::mutex> lk(mu_);
  level_ = level;
}

Level Logger::GetLevel() const {
  std::lock_guard<std::mutex> lk(mu_);
  return level_;
}

bool Logger::ShouldLog(Level level) const {
  return static_cast<std::uint8_t>(level) >= static_cast<std::uint8_t>(level_);
}

void Logger::Log(Level level, const std::string& msg) {
  Log(level, std::string{}, msg);
}

void Logger::Log(Level level, const std::string& tag, const std::string& msg) {
  std::shared_ptr<Sink> sink;
  Event e;

  {
    std::lock_guard<std::mutex> lk(mu_);
    if (!sink_ || !ShouldLog(level)) return;
    sink = sink_;
    e.level = level;
    e.ts = std::chrono::system_clock::now();
    e.tag = tag;
    e.message = msg;
  }

  sink->Write(e);
}

void Logger::Trace(const std::string& msg) { Log(Level::Trace, msg); }
void Logger::Debug(const std::string& msg) { Log(Level::Debug, msg); }
void Logger::Info(const std::string& msg) { Log(Level::Info, msg); }
void Logger::Warn(const std::string& msg) { Log(Level::Warn, msg); }
void Logger::Error(const std::string& msg) { Log(Level::Error, msg); }
void Logger::Fatal(const std::string& msg) { Log(Level::Fatal, msg); }

void Logger::Flush() {
  std::shared_ptr<Sink> sink;
  {
    std::lock_guard<std::mutex> lk(mu_);
    sink = sink_;
  }
  if (sink) sink->Flush();
}

FileSink::FileSink(std::string file_path) : file_path_(std::move(file_path)) {}

std::string FileSink::Path() const {
  std::lock_guard<std::mutex> lk(mu_);
  return file_path_;
}

std::string FileSink::FormatLine(const Event& e) const {
  const auto tt = std::chrono::system_clock::to_time_t(e.ts);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &tt);
#else
  localtime_r(&tt, &tm);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  oss << " [" << ToString(e.level) << "]";
  if (!e.tag.empty()) oss << " [" << e.tag << "]";
  oss << " " << e.message << "\n";
  return oss.str();
}

void FileSink::Write(const Event& e) {
  std::lock_guard<std::mutex> lk(mu_);

  std::ofstream ofs(file_path_.c_str(), std::ios::out | std::ios::app);
  if (!ofs) return;

  ofs << FormatLine(e);
  ofs.flush();
}

void FileSink::Flush() {
  std::lock_guard<std::mutex> lk(mu_);
  std::ofstream ofs(file_path_.c_str(), std::ios::out | std::ios::app);
  if (!ofs) return;
  ofs.flush();
}

}  // namespace iotgw::core::common::log