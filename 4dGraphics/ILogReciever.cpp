#include "ILogReciever.hpp"

#include "v4dgCore.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <source_location>
#include <string_view>
#include <syncstream>
#include <typeinfo>
#include <ranges>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace {
void standard_format_header(auto &it, std::source_location lc, v4dg::ILogReciever::LogLevel lv) {
  std::string_view file_name = lc.file_name();
  file_name = file_name.substr(file_name.find_last_of("/\\") + 1); // extract file name from path

  std::string_view fn_name = lc.function_name();
  
  auto s = fn_name.find_first_of(" ");
  if ( s == std::string_view::npos )
    s = 0;
  else
    ++s;
  
  auto e = fn_name.find_first_of("(", s);
  
  fn_name = fn_name.substr(s, e - s);
  std::format_to(it, "{:<8} {}({}):{}: ", lv, file_name, fn_name, lc.line());
}
}

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

void NullLogReciever::do_log(std::string_view, std::format_args,
                             std::source_location, LogLevel) {
  return;
}

FileLogReciever::FileLogReciever(const std::filesystem::path &path)
    : m_file(path), m_epoch(std::chrono::steady_clock::now()) {
  if (!m_file.is_open())
    throw exception("Failed to open log file \"{}\" for writing", path.native());

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
  standard_format_header(it, lc, lv);
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
  std::string str;
  auto it = std::back_inserter(str);
  standard_format_header(it, lc, lv);
  std::vformat_to(it, fmt, args);
  MessageBoxA(nullptr, str.c_str(), "4dGraphics", MB_OK);
}
#endif

MultiLogReciever::MultiLogReciever(
    std::span<const std::shared_ptr<ILogReciever>> recievers) {
  m_recievers.reserve(recievers.size());
  for (auto &reciever :
       recievers | std::views::filter([](auto &r) { return !!r; })) {
    // Flatten MultiLogRecievers
    if (typeid(*reciever) == typeid(MultiLogReciever)) {
      auto &other = static_cast<MultiLogReciever &>(*reciever).m_recievers;
      m_recievers.insert(m_recievers.end(), other.begin(), other.end());
    } else {
      m_recievers.push_back(std::move(reciever));
    }
  }

  m_recievers.shrink_to_fit();
}

void MultiLogReciever::do_log(std::string_view fmt, std::format_args args,
                              std::source_location lc, LogLevel lv) {
  // only format once
  std::string formatted = std::vformat(fmt, args);
  for (auto &reciever : m_recievers)
    reciever->log("{}", std::make_format_args(formatted), lc, lv);
}

const std::shared_ptr<NullLogReciever> nullLogReciever =
    std::make_shared<NullLogReciever>();
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