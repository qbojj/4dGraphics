#include "ILogReciever.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <print>
#include <ranges>
#include <source_location>
#include <string_view>
#include <syncstream>
#include <typeinfo>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace {
void standard_format_header(auto &it, std::source_location lc,
                            v4dg::ILogReciever::LogLevel lv,
                            bool remove_type = true, std::size_t max_len = 40) {
  std::string_view file_name = lc.file_name();

  auto pos = file_name.find_last_of("/\\");
  if (pos != std::string_view::npos)
    file_name = file_name.substr(pos + 1); // extract file name from path

  std::string_view fn_name = lc.function_name();

  if (remove_type && fn_name.contains(' ')) {
    // remove type (it may contain templates and we do not
    // want spaces in them to interfere with the formatting)
    // e.g std::vector<int, std::allocator<int> > fn() -> fn()

    std::size_t s = 0;
    int par_level = 0;
    while (s < fn_name.size()) {
      if (fn_name[s] == '<')
        ++par_level;
      else if (fn_name[s] == '>')
        --par_level;
      else if (par_level == 0 && fn_name[s] == ' ')
        break;
      ++s;
    }

    s = fn_name.find_first_not_of(" ", s);

    if (s != std::string_view::npos)
      fn_name = fn_name.substr(s);
  }

  std::format_to(it, "{:<8} {}({:.{}}{}):{}: ", lv, file_name, fn_name, max_len,
                 fn_name.size() > max_len ? "..." : "", lc.line());
}
} // namespace

namespace v4dg {
std::string to_string(ILogReciever::LogLevel lv) {
  using enum ILogReciever::LogLevel;

  switch (lv) {
  case Debug:
    return "Debug";
  case Log:
    return "Log";
  case Warning:
    return "Warning";
  case Error:
    return "Error";
  case FatalError:
    return "FatalError";
  case PrintAlways:
    return "PrintAlways";
  default:
    return std::format("Unknown({:x})", static_cast<std::uint8_t>(lv));
  }
}

FileLogReciever::FileLogReciever(const std::filesystem::path &path)
    : m_file(path), m_epoch(std::chrono::steady_clock::now()) {
  m_file.exceptions(std::ios::failbit | std::ios::badbit);

  m_file << std::format("Log file opened at {}\n",
                        std::chrono::system_clock::now());
}

void FileLogReciever::do_log(std::string_view fmt, std::format_args args,
                             std::source_location lc, LogLevel lv) {
  auto now = std::chrono::steady_clock::now();
  auto time =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - m_epoch);

  auto ss = std::osyncstream(m_file);
  auto it = std::ostreambuf_iterator<char>(ss);
  std::format_to(it, "[{:%Q%q}] ", time);
  standard_format_header(it, lc, lv, false, 1024);
  std::vformat_to(it, fmt, args);
  it = '\n';
  ss.flush();
}

void CerrLogReciever::do_log(std::string_view fmt, std::format_args args,
                             std::source_location lc, LogLevel lv) {
  auto ss = std::osyncstream(std::cerr);
  auto it = std::ostreambuf_iterator<char>(ss);
  standard_format_header(it, lc, lv);
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

MultiLogReciever::MultiLogReciever(
    std::span<const std::shared_ptr<ILogReciever>> recievers) {
  m_recievers.reserve(recievers.size());
  for (auto &reciever :
       recievers | std::views::filter([](auto &r) { return bool{r}; })) {
    // Flatten MultiLogRecievers
    auto *multi_reciever = dynamic_cast<MultiLogReciever *>(reciever.get());
    if (multi_reciever) {
      m_recievers.insert(m_recievers.end(), multi_reciever->m_recievers.begin(),
                         multi_reciever->m_recievers.end());
      continue;
    }

    m_recievers.push_back(reciever);
  }

  m_recievers.shrink_to_fit();
}

void MultiLogReciever::do_log(std::string_view fmt, std::format_args args,
                              std::source_location lc, LogLevel lv) {
  for (auto &reciever : m_recievers)
    reciever->log(fmt, args, lc, lv);
}

const std::shared_ptr<CerrLogReciever> cerrLogReciever =
    std::make_shared<CerrLogReciever>();
#ifdef _WIN32
const std::shared_ptr<OutputDebugStringLogReciever>
    outputDebugStringLogReciever =
        std::make_shared<OutputDebugStringLogReciever>();
const std::shared_ptr<MessageBoxLogReciever> messageBoxLogReciever =
    std::make_shared<MessageBoxLogReciever>();
#endif

} // namespace v4dg