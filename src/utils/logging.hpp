#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <cstdint>

namespace hic {

class Logger {
public:
  enum class Level {
    DEBUG,
    INFO,
    WARN,
    ERROR
  };

  explicit Logger(const std::string& name) : name(name) {
    const size_t hash = std::hash<std::string>{}(name);
    r = (hash >> 16) & 0xFF;
    g = (hash >> 8) & 0xFF;
    b = hash & 0xFF;
  }

  template<typename... Args>
  void debug(Args&&... args) {
      log(Level::DEBUG, std::forward<Args>(args)...);
  }

  template<typename... Args>
  void info(Args&&... args) {
      log(Level::INFO, std::forward<Args>(args)...);
  }

  template<typename... Args>
  void warn(Args&&... args) {
      log(Level::WARN, std::forward<Args>(args)...);
  }

  template<typename... Args>
  void error(Args&&... args) {
      log(Level::ERROR, std::forward<Args>(args)...);
  }
private:
  std::string name;
  uint8_t r, g, b;

  template<typename... Args>
  void log(const Level level, Args&&... args) {
    std::ostringstream oss;

    oss <<
      "\033[38;2;" <<
        static_cast<int>(r) << ";" <<
        static_cast<int>(g) << ";" <<
        static_cast<int>(b) << "m";
    oss << "[" << name << "]";
    oss << "\033[0m ";

    switch (level) {
      case Level::DEBUG: oss << "\033[90m[DBG]\033[0m "; break;
      case Level::INFO:  oss << "\033[34m[INF]\033[0m "; break;
      case Level::WARN:  oss << "\033[33m[WRN]\033[0m "; break;
      case Level::ERROR: oss << "\033[31m[ERR]\033[0m "; break;
    }

    (oss << ... << args);
    std::cout << oss.str() << std::endl;
  }
};

} // namespace hic