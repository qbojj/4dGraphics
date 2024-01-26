#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <mutex>
#include <source_location>
#include <span>
#include <string_view>
#include <vector>

namespace v4dg {
class ILogReciever {
public:
  enum class LogLevel : std::uint8_t {
    Debug,
    Log,
    Warning,
    Error,
    FatalError,
    PrintAlways = std::numeric_limits<std::uint8_t>::max()
  };

  void log(std::string_view fmt, std::format_args args, std::source_location lc,
           LogLevel lv) {
    do_log(fmt, args, lc, lv);
  }

private:
  virtual void do_log(std::string_view fmt, std::format_args args,
                      std::source_location lc, LogLevel lv) = 0;
};

std::string to_string(ILogReciever::LogLevel lv);

class NullLogReciever final : public ILogReciever {
private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location lc, LogLevel lv) override;
};

class FileLogReciever : public ILogReciever {
public:
  FileLogReciever(const std::filesystem::path &path);

private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location lc, LogLevel lv) override;

  std::ofstream m_file;
  std::chrono::steady_clock::time_point m_epoch;
};

class CerrLogReciever : public ILogReciever {
private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location lc, LogLevel lv) override;
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
  MultiLogReciever(std::span<const std::shared_ptr<ILogReciever>> recievers);

private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location lc, LogLevel lv) override;

  std::vector<std::shared_ptr<ILogReciever>> m_recievers;
};

extern const std::shared_ptr<NullLogReciever> nullLogReciever;
extern const std::shared_ptr<CerrLogReciever> cerrLogReciever;
#ifdef _WIN32
extern const std::shared_ptr<OutputDebugStringLogReciever>
    outputDebugStringLogReciever;
extern const std::shared_ptr<MessageBoxLogReciever> messageBoxLogReciever;
#endif
} // namespace v4dg

namespace std {
template <>
struct formatter<v4dg::ILogReciever::LogLevel> : formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const v4dg::ILogReciever::LogLevel &lv, FormatContext &ctx) const {
    return formatter<std::string_view>::format(v4dg::to_string(lv), ctx);
  }
};
} // namespace std