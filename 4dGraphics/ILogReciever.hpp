#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace v4dg {
class ILogReciever {
public:
  ILogReciever() = default;
  ILogReciever(ILogReciever&) = delete;
  ILogReciever(ILogReciever&&) = delete;
  ILogReciever& operator=(ILogReciever&) = delete;
  ILogReciever& operator=(ILogReciever&&) = delete;

  virtual ~ILogReciever() = default;

  enum class LogLevel : std::uint8_t {
    Debug = 0,
    Log = 1,
    Warning = 2,
    Error = 3,
    FatalError = 4,
    PrintAlways = std::numeric_limits<std::uint8_t>::max()
  };

  void log(std::string_view fmt, std::format_args args,
           std::source_location loc, LogLevel lev) {
    do_log(fmt, args, loc, lev);
  }

private:
  virtual void do_log(std::string_view fmt, std::format_args args,
                      std::source_location loc, LogLevel lev) = 0;
};

std::string to_string(ILogReciever::LogLevel lev);

class FileLogReciever : public ILogReciever {
public:
  explicit FileLogReciever(const std::filesystem::path &path);

private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location loc, LogLevel lev) override;

  std::ofstream m_file;
  std::chrono::steady_clock::time_point m_epoch;
};

class CerrLogReciever : public ILogReciever {
private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location loc, LogLevel lev) override;
};

#ifdef _WIN32
class OutputDebugStringLogReciever : public ILogReciever {
private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location lc, LogLevel lv) override;
};

class MessageBoxLogReciever : public ILogReciever {
private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location lc, LogLevel lv) override;
};
#endif

class MultiLogReciever final : public ILogReciever {
public:
  static std::shared_ptr<ILogReciever>
  from_span(std::span<const std::shared_ptr<ILogReciever>> recievers);

  MultiLogReciever(std::span<const std::shared_ptr<ILogReciever>> recievers);

private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location loc, LogLevel lev) override;

  std::vector<std::shared_ptr<ILogReciever>> m_recievers;
};
} // namespace v4dg

namespace std {
template <>
struct formatter<v4dg::ILogReciever::LogLevel> : formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const v4dg::ILogReciever::LogLevel &lev,
              FormatContext &ctx) const {
    return formatter<std::string_view>::format(v4dg::to_string(lev), ctx);
  }
};
} // namespace std
