#pragma once

#include "ILogReciever.hpp"

#include <atomic>
#include <concepts>
#include <exception>
#include <format>
#include <iostream>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace v4dg {
class Logger {
public:
  using LogLevel = ILogReciever::LogLevel;

  template <class CharT, class... Args>
  class basic_format_string_with_location {
  public:
    std::basic_format_string<CharT, Args...> fmt;
    std::source_location lc;

    template <typename T>
      requires std::convertible_to<const T &, std::basic_string<CharT>>
    consteval basic_format_string_with_location(
        const T &fmt, std::source_location lc = std::source_location::current())
        : fmt(fmt), lc(lc) {}

    basic_format_string_with_location(
        std::basic_format_string<CharT, Args...> fmt,
        std::source_location lc = std::source_location::current())
        : fmt(fmt), lc(lc) {}
  };

  template <class... Args>
  using format_string_with_location = typename std::type_identity_t<
      basic_format_string_with_location<char, Args...>>;

  template <class... Args>
  using wformat_string_with_location = typename std::type_identity_t<
      basic_format_string_with_location<wchar_t, Args...>>;

  Logger(LogLevel lv = LogLevel::Log,
         std::shared_ptr<ILogReciever> reciever = nullptr)
      : m_logLevel(lv), m_logReciever(std::move(reciever)) {}

  template <class... Args>
  void Debug(format_string_with_location<Args...> fmt,
             Args &&...args) noexcept {
    GenericLog(LogLevel::Debug, fmt, std::forward<Args>(args)...);
  }

  void
  DebugV(std::string_view fmt, std::format_args args,
         std::source_location lc = std::source_location::current()) noexcept {
    GenericLogV(LogLevel::Debug, fmt, args, lc);
  }

  template <class... Args>
  void Log(format_string_with_location<Args...> fmt, Args &&...args) noexcept {
    GenericLog(LogLevel::Log, fmt, std::forward<Args>(args)...);
  }

  void
  LogV(std::string_view fmt, std::format_args args,
       std::source_location lc = std::source_location::current()) noexcept {
    GenericLogV(LogLevel::Log, fmt, args, lc);
  }

  template <class... Args>
  void Warning(format_string_with_location<Args...> fmt,
               Args &&...args) noexcept {
    GenericLog(LogLevel::Warning, fmt, std::forward<Args>(args)...);
  }

  void
  WarningV(std::string_view fmt, std::format_args args,
           std::source_location lc = std::source_location::current()) noexcept {
    GenericLogV(LogLevel::Warning, fmt, args, lc);
  }

  template <class... Args>
  void Error(format_string_with_location<Args...> fmt,
             Args &&...args) noexcept {
    GenericLog(LogLevel::Error, fmt, std::forward<Args>(args)...);
  }

  void
  ErrorV(std::string_view fmt, std::format_args args,
         std::source_location lc = std::source_location::current()) noexcept {
    GenericLogV(LogLevel::Error, fmt, args, lc);
  }

  template <class... Args>
  void FatalError(format_string_with_location<Args...> fmt,
                  Args &&...args) noexcept {
    GenericLog(LogLevel::FatalError, fmt, std::forward<Args>(args)...);
  }

  void FatalErrorV(
      std::string_view fmt, std::format_args args,
      std::source_location lc = std::source_location::current()) noexcept {
    GenericLogV(LogLevel::FatalError, fmt, args, lc);
  }

  template <class... Args>
  void Always(format_string_with_location<Args...> fmt,
              Args &&...args) noexcept {
    GenericLog(LogLevel::PrintAlways, fmt, std::forward<Args>(args)...);
  }

  void
  AlwaysV(std::string_view fmt, std::format_args args,
          std::source_location lc = std::source_location::current()) noexcept {
    GenericLogV(LogLevel::PrintAlways, fmt, args, lc);
  }

  // NOLINTBEGIN(cppcoreguidelines-missing-std-forward)
  template <class... Args>
  void GenericLog(LogLevel lv, format_string_with_location<Args...> fmt,
                  Args &&...args) noexcept {
    GenericLogV(lv, fmt.fmt.get(), std::make_format_args(args...), fmt.lc);
  }
  // NOLINTEND(cppcoreguidelines-missing-std-forward)

  void GenericLogV(
      LogLevel lv, std::string_view fmt, std::format_args args,
      std::source_location lc = std::source_location::current()) noexcept {
    try {
      if (lv >= getLogLevel() && m_logReciever) {
        m_logReciever->log(fmt, args, lc, lv);
      }
    } catch (const std::exception &e) {
      std::cerr << "Exception caught in log reciever: " << e.what() << '\n';
    } catch (...) {
      std::cerr << "Unknown Exception caught in log reciever\n";
    }
  }

  [[nodiscard]] LogLevel getLogLevel() const noexcept {
    return m_logLevel.load(std::memory_order::relaxed);
  }
  void setLogLevel(LogLevel lv) noexcept {
    m_logLevel.store(lv, std::memory_order::relaxed);
  }

  // it is not safe to call this function when multiple threads are logging
  void setLogReciever(std::shared_ptr<ILogReciever> reciever) {
    m_logReciever = std::move(reciever);
  }

private:
  std::atomic<LogLevel> m_logLevel;
  std::shared_ptr<ILogReciever> m_logReciever;
};

extern Logger logger;
} // namespace v4dg