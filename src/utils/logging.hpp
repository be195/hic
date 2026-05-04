#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <cstdint>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <atomic>

namespace hic {

class Logger {
public:
  enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERR
  };

  explicit Logger(const std::string& name) : name(name) {
    const size_t hash = std::hash<std::string>{}(name);
    r = (hash >> 16) & 0xFF;
    g = (hash >> 8) & 0xFF;
    b = hash & 0xFF;
  }

  template<typename... Args>
  void debug(Args&&... args) {
    log(LogLevel::DEBUG, std::forward<Args>(args)...);
  }

  template<typename... Args>
  void info(Args&&... args) {
    log(LogLevel::INFO, std::forward<Args>(args)...);
  }

  template<typename... Args>
  void warn(Args&&... args) {
    log(LogLevel::WARN, std::forward<Args>(args)...);
  }

  template<typename... Args>
  void error(Args&&... args) {
    log(LogLevel::ERR, std::forward<Args>(args)...);
  }

  static Logger& getShutdownLoggerDummy() {
    static Logger dummy("SHUTDOWN");
    return dummy;
  }
  static Logger& get(const std::string& name) {
    if (isShuttingDown)
      return getShutdownLoggerDummy();

    static std::mutex mutex;
    static std::unordered_map<std::string, std::unique_ptr<Logger>> loggers;

    std::lock_guard lock(mutex);
    auto it = loggers.find(name);
    if (it == loggers.end())
      it = loggers.emplace(name, std::unique_ptr<Logger>(new Logger(name))).first;
    return *it->second;
  }

  static void signalShutdown() {
    isShuttingDown = true;
  }

  static bool alive() {
    return !isShuttingDown;
  }

  static void setMinLevel(const LogLevel level) {
    minLevel = level;
  }

  static void setColored(const bool enable) {
    colored = enable;
  }

private:
  std::string name;
  uint8_t r, g, b;

  static inline std::atomic<LogLevel> minLevel{LogLevel::DEBUG};
  static inline std::atomic<bool> colored{true};
  static inline std::mutex outputMutex;
  static inline std::atomic<bool> isShuttingDown{false};

  template<typename... Args>
  void log(const LogLevel level, Args&&... args) {
    if (level < minLevel) return;

    std::ostringstream oss;

    if (colored)
      oss <<
        "\033[38;2;" <<
          static_cast<int>(r) << ";" <<
          static_cast<int>(g) << ";" <<
          static_cast<int>(b) << "m";

    oss << "[" << name << "]";

    if (colored) oss << "\033[0m";
    oss << " ";

    if (colored)
      switch (level) {
        case LogLevel::DEBUG: oss << "\033[90m[DBG]\033[0m "; break;
        case LogLevel::INFO:  oss << "\033[34m[INF]\033[0m "; break;
        case LogLevel::WARN:  oss << "\033[33m[WRN]\033[0m "; break;
        case LogLevel::ERR: oss << "\033[31m[ERR]\033[0m "; break;
      }
    else
      switch (level) {
        case LogLevel::DEBUG: oss << "[DBG] "; break;
        case LogLevel::INFO:  oss << "[INF] "; break;
        case LogLevel::WARN:  oss << "[WRN] "; break;
        case LogLevel::ERR: oss << "[ERR] "; break;
      }

    bool first = true;
    ((oss << (first ? "" : " ") << args, first = false), ...);

    std::lock_guard lock(outputMutex);
    std::cerr << oss.str() << std::endl;
    std::cerr.flush();
  }
};

} // namespace hic

#define HICL(name) hic::Logger::get(name)