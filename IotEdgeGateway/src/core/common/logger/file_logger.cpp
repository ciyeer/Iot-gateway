#include "core/common/logger/logger.hpp"

#include <ctime>
#include "core/common/utils/std_compat.hpp"
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

void Logger::Log(Level level, std::string_view msg) {
  Log(level, std::string_view{}, msg);
}

void Logger::Log(Level level, std::string_view tag, std::string_view msg) {
  std::shared_ptr<Sink> sink;
  Event e;

  {
    std::lock_guard<std::mutex> lk(mu_);
    if (!sink_ || !ShouldLog(level)) return;
    sink = sink_;
    e.level = level;
    e.ts = std::chrono::system_clock::now();
    e.tag.assign(tag.data(), tag.size());
    e.message.assign(msg.data(), msg.size());
  }

  sink->Write(e);
}

void Logger::Trace(std::string_view msg) { Log(Level::Trace, msg); }
void Logger::Debug(std::string_view msg) { Log(Level::Debug, msg); }
void Logger::Info(std::string_view msg) { Log(Level::Info, msg); }
void Logger::Warn(std::string_view msg) { Log(Level::Warn, msg); }
void Logger::Error(std::string_view msg) { Log(Level::Error, msg); }
void Logger::Fatal(std::string_view msg) { Log(Level::Fatal, msg); }

void Logger::Flush() {
  std::shared_ptr<Sink> sink;
  {
    std::lock_guard<std::mutex> lk(mu_);
    sink = sink_;
  }
  if (sink) sink->Flush();
}

FileSink::FileSink(std::filesystem::path file_path) : file_path_(std::move(file_path)) {}

std::filesystem::path FileSink::Path() const {
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

  std::ofstream ofs(file_path_, std::ios::out | std::ios::app);
  if (!ofs) return;

  ofs << FormatLine(e);
  ofs.flush();
}

void FileSink::Flush() {
  std::lock_guard<std::mutex> lk(mu_);
  std::ofstream ofs(file_path_, std::ios::out | std::ios::app);
  if (!ofs) return;
  ofs.flush();
}

}  // namespace iotgw::core::common::log