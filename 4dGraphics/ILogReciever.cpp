#include "ILogReciever.hpp"

#include "v4dgCore.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>
#include <string_view>
#include <syncstream>
#include <typeinfo>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

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

FileLogReciever::FileLogReciever(std::filesystem::path path)
    : m_file(path), m_epoch(std::chrono::steady_clock::now()) {
  if (!m_file.is_open()) {
    throw std::runtime_error(std::format(
        "Failed to open log file \"{}\" for writing", path.native()));
  }

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
  std::format_to(it, "[{:%Q%q}] {:<12} {}({}):{}: ", time, lv, lc.file_name(),
                 lc.function_name(), lc.line());
  std::vformat_to(it, fmt, args);
  it = '\n';
}

void CerrLogReciever::do_log(std::string_view fmt, std::format_args args,
                             std::source_location lc, LogLevel lv) {
  auto ss = std::osyncstream(std::cerr);
  auto it = std::ostreambuf_iterator<char>(ss);
  std::format_to(it, "{:<12} {}({}):{}: ", lv, lc.file_name(),
                 lc.function_name(), lc.line());
  std::vformat_to(it, fmt, args);
  it = '\n';
}

#ifdef _WIN32
void OutputDebugStringLogReciever::do_log(std::string_view fmt,
                                          std::format_args args,
                                          std::source_location lc,
                                          LogLevel lv) {
  std::string str;
  auto it = std::back_inserter(str);
  std::format_to(it, "{:<12} {}({}):{}: ", lv, lc.file_name(),
                 lc.function_name(), lc.line());
  std::vformat_to(it, fmt, args);
  OutputDebugStringA(str.c_str());
}

void MessageBoxLogReciever::do_log(std::string_view fmt, std::format_args args,
                                   std::source_location lc, LogLevel lv) {
  std::string str;
  auto it = std::back_inserter(str);
  std::format_to(it, "{:<12} {}({}):{}: ", lv, lc.file_name(),
                 lc.function_name(), lc.line());
  std::vformat_to(it, fmt, args);
  MessageBoxA(nullptr, str.c_str(), "4dGraphics", MB_OK);
}
#endif

MultiLogReciever::MultiLogReciever(
    std::vector<std::shared_ptr<ILogReciever>> recievers) {
  m_recievers.reserve(recievers.size());
  for (auto &reciever : recievers) {
    if (!reciever)
      continue;

    // Flatten MultiLogRecievers
    if (typeid(*reciever) == typeid(MultiLogReciever)) {
      auto &multi = static_cast<MultiLogReciever &>(*reciever);
      m_recievers.insert(m_recievers.end(), multi.m_recievers.begin(),
                         multi.m_recievers.end());
    } else {
      m_recievers.push_back(std::move(reciever));
    }
  }

  m_recievers.shrink_to_fit();
}

void MultiLogReciever::do_log(std::string_view fmt, std::format_args args,
                              std::source_location lc, LogLevel lv) {
  for (auto &reciever : m_recievers)
    reciever->log(fmt, args, lc, lv);
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