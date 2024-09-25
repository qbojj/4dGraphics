module;

#include <tracy/Tracy.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <ranges>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <syncstream>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

export module v4dg.logger;

namespace v4dg {
export class ILogReciever {
public:
  ILogReciever() = default;
  ILogReciever(ILogReciever &) = delete;
  ILogReciever(ILogReciever &&) = delete;
  ILogReciever &operator=(ILogReciever &) = delete;
  ILogReciever &operator=(ILogReciever &&) = delete;

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

export class Logger {
public:
  using LogLevel = ILogReciever::LogLevel;

  template <class CharT, class... Args>
  class basic_format_string_with_location {
  public:
    template <typename T>
      requires std::convertible_to<const T &, std::basic_string<CharT>>
    consteval basic_format_string_with_location(
        const T &fmt, std::source_location lc = std::source_location::current())
        : fmt(fmt), lc(lc) {}

    basic_format_string_with_location(
        std::basic_format_string<CharT, Args...> fmt,
        std::source_location lc = std::source_location::current())
        : fmt(fmt), lc(lc) {}

    auto get() const noexcept { return fmt; }
    auto location() const noexcept { return lc; }

  private:
    std::basic_format_string<CharT, Args...> fmt;
    std::source_location lc;
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
    GenericLogV(lv, fmt.get().get(), std::make_format_args(args...),
                fmt.location());
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

export Logger logger;

export class FileLogReciever : public ILogReciever {
public:
  explicit FileLogReciever(const std::filesystem::path &path);

private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location loc, LogLevel lev) override;

  std::ofstream m_file;
  std::chrono::steady_clock::time_point m_epoch;
};

export class CerrLogReciever : public ILogReciever {
private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location loc, LogLevel lev) override;
};

#ifdef _WIN32
export class OutputDebugStringLogReciever : public ILogReciever {
private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location lc, LogLevel lv) override;
};

export class MessageBoxLogReciever : public ILogReciever {
private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location lc, LogLevel lv) override;
};
#endif

export class MultiLogReciever final : public ILogReciever {
public:
  static std::shared_ptr<ILogReciever>
  from_span(std::span<const std::shared_ptr<ILogReciever>> recievers);

  MultiLogReciever(std::span<const std::shared_ptr<ILogReciever>> recievers);

private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location loc, LogLevel lev) override;

  std::vector<std::shared_ptr<ILogReciever>> m_recievers;
};

export class TracyLogReciever : public v4dg::ILogReciever {
public:
  TracyLogReciever() = default;

private:
  void do_log(std::string_view fmt, std::format_args args,
              std::source_location loc, LogLevel lev) override;
};
} // namespace v4dg

namespace std {
export template <>
struct formatter<v4dg::ILogReciever::LogLevel> : formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const v4dg::ILogReciever::LogLevel &lev,
              FormatContext &ctx) const {
    using enum v4dg::ILogReciever::LogLevel;

    auto format = [&](auto sv) {
      return formatter<std::string_view>::format(sv, ctx);
    };

    switch (lev) {
    case Debug:
      return format("Debug");
    case Log:
      return format("Log");
    case Warning:
      return format("Warning");
    case Error:
      return format("Error");
    case FatalError:
      return format("FatalError");
    case PrintAlways:
      return format("PrintAlways");
    default:
      return format(
          std::format("Unknown({:x})", static_cast<std::uint8_t>(lev)));
    }
  }
};
} // namespace std

module :private;

constexpr auto max_format_len = 40;
constexpr auto max_file_header_formatter_len = 1024;

void standard_format_header(auto &it, std::source_location loc,
                            v4dg::ILogReciever::LogLevel lev,
                            bool remove_type = true,
                            std::size_t max_len = max_format_len) {
  std::string_view file_name = loc.file_name();

  auto pos = file_name.find_last_of("/\\");
  if (pos != std::string_view::npos) {
    file_name = file_name.substr(pos + 1); // extract file name from path
  }

  std::string_view fn_name = loc.function_name();

  if (remove_type && fn_name.contains(' ')) {
    // remove type (it may contain templates and we do not
    // want spaces in them to interfere with the formatting)
    // e.g std::vector<int, std::allocator<int> > fn() -> fn()

    std::size_t s = 0;
    int par_level = 0;
    while (s < fn_name.size()) {
      if (fn_name[s] == '<') {
        ++par_level;
      } else if (fn_name[s] == '>') {
        --par_level;
      } else if (par_level == 0 && fn_name[s] == ' ') {
        break;
      }
      ++s;
    }

    s = fn_name.find_first_not_of(' ', s);

    if (s != std::string_view::npos) {
      fn_name = fn_name.substr(s);
    }
  }

  std::format_to(it, "{:<8} {}({:.{}}{}):{}: ", lev, file_name, fn_name,
                 max_len, fn_name.size() > max_len ? "..." : "", loc.line());
}

using namespace v4dg;
FileLogReciever::FileLogReciever(const std::filesystem::path &path)
    : m_file(path), m_epoch(std::chrono::steady_clock::now()) {
  m_file.exceptions(std::ios::failbit | std::ios::badbit);

  m_file << std::format("Log file opened at {}\n",
                        std::chrono::system_clock::now());
}

void FileLogReciever::do_log(std::string_view fmt, std::format_args args,
                             std::source_location loc, LogLevel lev) {
  auto now = std::chrono::steady_clock::now();
  auto time =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - m_epoch);

  auto ss = std::osyncstream(m_file);
  auto it = std::ostreambuf_iterator<char>(ss);
  std::format_to(it, "[{:%Q%q}] ", time);
  standard_format_header(it, loc, lev, false, max_file_header_formatter_len);
  std::vformat_to(it, fmt, args);
  it = '\n';
  ss.flush();
}

void CerrLogReciever::do_log(std::string_view fmt, std::format_args args,
                             std::source_location loc, LogLevel lev) {
  auto ss = std::osyncstream(std::cerr);
  auto it = std::ostreambuf_iterator<char>(ss);
  standard_format_header(it, loc, lev);
  std::vformat_to(it, fmt, args);
  it = '\n';
  ss.flush();
}

#ifdef _WIN32
void OutputDebugStringLogReciever::do_log(std::string_view fmt,
                                          std::format_args args,
                                          std::source_location lc,
                                          LogLevel lv) {
  std::string str;
  auto it = std::back_inserter(str);
  standard_format_header(it, lc, lv);
  std::vformat_to(it, fmt, args);
  OutputDebugStringA(str.c_str());
}

void MessageBoxLogReciever::do_log(std::string_view fmt, std::format_args args,
                                   std::source_location lc, LogLevel lv) {
  std::string caption;
  auto it = std::back_inserter(caption);
  standard_format_header(it, lc, lv);

  std::string msg = std::vformat(it, fmt, args);
  MessageBoxA(nullptr, msg.c_str(), caption.c_str(), MB_OK);
}
#endif

std::shared_ptr<ILogReciever> MultiLogReciever::from_span(
    std::span<const std::shared_ptr<ILogReciever>> recievers) {
  if (recievers.empty()) {
    return {};
  }

  if (recievers.size() == 1) {
    return recievers[0];
  }

  return std::make_shared<MultiLogReciever>(recievers);
}

MultiLogReciever::MultiLogReciever(
    std::span<const std::shared_ptr<ILogReciever>> recievers) {
  m_recievers.reserve(recievers.size());
  for (const auto &reciever :
       recievers | std::views::filter([](auto &rec) { return bool{rec}; })) {
    // Flatten MultiLogRecievers
    auto *multi_reciever = dynamic_cast<MultiLogReciever *>(reciever.get());
    if (multi_reciever != nullptr) {
      m_recievers.insert(m_recievers.end(), multi_reciever->m_recievers.begin(),
                         multi_reciever->m_recievers.end());
      continue;
    }

    m_recievers.push_back(reciever);
  }

  m_recievers.shrink_to_fit();
}

void MultiLogReciever::do_log(std::string_view fmt, std::format_args args,
                              std::source_location loc, LogLevel lev) {
  for (auto &reciever : m_recievers) {
    reciever->log(fmt, args, loc, lev);
  }
}

void TracyLogReciever::do_log(std::string_view fmt, std::format_args args,
                              std::source_location loc, LogLevel lev) {
  std::string str;
  auto it = std::back_inserter(str);
  standard_format_header(it, loc, lev);
  std::vformat_to(it, fmt, args);
  TracyMessage(str.c_str(), str.size());
}
