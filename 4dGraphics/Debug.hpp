#pragma once

#include "ILogReciever.hpp"

#include <atomic>
#include <format>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <source_location>
#include <string_view>

namespace v4dg {
class Logger {
public:
  using LogLevel = ILogReciever::LogLevel;

  template <class CharT, class... Args>
  class basic_format_string_with_location {
  public:
    std::basic_format_string<CharT, Args...> fmt;
    std::source_location lc;

    template <class T>
    consteval basic_format_string_with_location(
        const T &s,
        const std::source_location lc = std::source_location::current())
        : fmt(s), lc(lc) {}
  };

  template <class... Args>
  using format_string_with_location = typename std::type_identity_t<
      basic_format_string_with_location<char, Args...>>;

  template <class... Args>
  using wformat_string_with_location = typename std::type_identity_t<
      basic_format_string_with_location<wchar_t, Args...>>;

  Logger(LogLevel lv = LogLevel::Log, std::shared_ptr<ILogReciever> reciever = nullLogReciever)
      : m_logLevel(lv), m_logReciever(reciever) {}

  template <class... Args>
  void Debug(format_string_with_location<Args...> fmt, Args &&...args) {
    GenericLog(LogLevel::Debug, fmt, std::forward<Args>(args)...);
  }

  void DebugV(std::string_view fmt, std::format_args args,
              std::source_location lc = std::source_location::current()) {
    GenericLogV(LogLevel::Debug, fmt, args, lc);
  }

  template <class... Args>
  void Log(format_string_with_location<Args...> fmt, Args &&...args) {
    GenericLog(LogLevel::Log, fmt, std::forward<Args>(args)...);
  }

  void LogV(std::string_view fmt, std::format_args args,
            std::source_location lc = std::source_location::current()) {
    GenericLogV(LogLevel::Log, fmt, args, lc);
  }

  template <class... Args>
  void Warning(format_string_with_location<Args...> fmt, Args &&...args) {
    GenericLog(LogLevel::Warning, fmt, std::forward<Args>(args)...);
  }

  void WarningV(std::string_view fmt, std::format_args args,
                std::source_location lc = std::source_location::current()) {
    GenericLogV(LogLevel::Warning, fmt, args, lc);
  }

  template <class... Args>
  void Error(format_string_with_location<Args...> fmt, Args &&...args) {
    GenericLog(LogLevel::Error, fmt, std::forward<Args>(args)...);
  }

  void ErrorV(std::string_view fmt, std::format_args args,
              std::source_location lc = std::source_location::current()) {
    GenericLogV(LogLevel::Error, fmt, args, lc);
  }

  template <class... Args>
  void FatalError(format_string_with_location<Args...> fmt, Args &&...args) {
    GenericLog(LogLevel::FatalError, fmt, std::forward<Args>(args)...);
  }

  void FatalErrorV(std::string_view fmt, std::format_args args,
                   std::source_location lc = std::source_location::current()) {
    GenericLogV(LogLevel::FatalError, fmt, args, lc);
  }

  template <class... Args>
  void Always(format_string_with_location<Args...> fmt, Args &&...args) {
    GenericLog(LogLevel::PrintAlways, fmt, std::forward<Args>(args)...);
  }

  void AlwaysV(std::string_view fmt, std::format_args args,
               std::source_location lc = std::source_location::current()) {
    GenericLogV(LogLevel::PrintAlways, fmt, args, lc);
  }

  template <class... Args>
  void GenericLog(LogLevel lv, format_string_with_location<Args...> fmt,
                  Args &&...args) {
    GenericLogV(lv, fmt.fmt.get(),
                std::make_format_args(std::forward<Args>(args)...), fmt.lc);
  }

  void GenericLogV(LogLevel lv, std::string_view fmt, std::format_args args,
                   std::source_location lc = std::source_location::current()) {
    if (lv < getLogLevel())
      return;
    
    m_logReciever->log(fmt, args, lc, lv);
  }

  LogLevel getLogLevel() const noexcept {
    return m_logLevel.load(std::memory_order::relaxed);
  }

  void setLogLevel(LogLevel lv) noexcept {
    m_logLevel.store(lv, std::memory_order_relaxed);
  }
  void setLogReciever(std::shared_ptr<ILogReciever> reciever) {
    m_logReciever = reciever;
  }

private:
  std::atomic<LogLevel> m_logLevel;
  std::shared_ptr<ILogReciever> m_logReciever;
};

extern Logger logger;
} // namespace v4dg