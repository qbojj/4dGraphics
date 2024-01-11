#include "Debug.hpp"
#include "GameHandler.hpp"
#include "ILogReciever.hpp"

#include <argparse/argparse.hpp>

#include <cstdlib>
#include <exception>
#include <format>
#include <memory>

v4dg::Logger v4dg::logger;

void parse_args(int argc, char *argv[]) {
  argparse::ArgumentParser parser;

  parser.add_argument("-d", "--debug-level")
      .help("set debug level. possible values (d, l, w, e, f, q)")
      .default_value('l');
  parser.add_argument("--log-path").help("set path of log file");
  parser.add_argument("-q", "--quiet")
      .help("disable logging to terminal")
      .default_value(false)
      .implicit_value(true);

#ifdef _WIN32
  parser.add_argument("--output-debug-string")
      .help("enable logging to OutputDebugString")
      .default_value(false)
      .implicit_value(true);

  parser.add_argument("--message-box")
      .help("enable logging to message box")
      .default_value(false)
      .implicit_value(true);
#endif

  try {
    parser.parse_args(argc, argv);
  } catch (const std::exception &e) {
    std::cout << e.what() << ' ' << parser;
    throw;
  }

  v4dg::Logger::LogLevel ll;

  switch (parser.get<char>("-d")) {
    using enum v4dg::Logger::LogLevel;
  case 'd':
    ll = Debug;
    break;
  case 'l':
    ll = Log;
    break;
  case 'w':
    ll = Warning;
    break;
  case 'e':
    ll = Error;
    break;
  case 'f':
    ll = FatalError;
    break;
  case 'q':
    ll = PrintAlways;
    break;
  default:
    throw std::runtime_error(
        std::format("unsupported debug level {}", parser.get<char>("-d")));
  }

  v4dg::logger.setLogLevel(ll);

  using sp_lr = std::shared_ptr<v4dg::ILogReciever>;
  std::vector<sp_lr> recievers;

  if (!parser.get<bool>("-q"))
    recievers.push_back(v4dg::cerrLogReciever);

  if (parser.is_used("--log-path")) {
    recievers.push_back(std::make_shared<v4dg::FileLogReciever>(
        parser.get<std::string>("--log-path")));
  }

#ifdef _WIN32
  if (parser.get<bool>("--output-debug-string"))
    recievers.push_back(v4dg::outputDebugStringLogReciever);

  if (parser.get<bool>("--message-box"))
    recievers.push_back(v4dg::messageBoxLogReciever);
#endif

  v4dg::logger.setLogReciever(
      recievers.size() == 0 ? v4dg::nullLogReciever
      : recievers.size() == 1
          ? std::move(recievers.front())
          : std::make_shared<v4dg::MultiLogReciever>(recievers));
}

#ifdef __cplusplus
extern "C"
#endif
int
main(int argc, char *argv[]) {

  std::srand((unsigned int)std::time(NULL));
  parse_args(argc, argv);

  return v4dg::MyGameHandler{}.Run();
}