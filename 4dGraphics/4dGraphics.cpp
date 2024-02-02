#include "Debug.hpp"
#include "GameHandler.hpp"
#include "ILogReciever.hpp"
#include "GameCore.hpp"

#include <argparse/argparse.hpp>
#include <SDL2/SDL_main.h>
#include <tracy/Tracy.hpp>

#include <cstdlib>
#include <exception>
#include <format>
#include <memory>
#include <new>

#ifdef DEBUG_ALLOCATIONS
void *operator new(std::size_t count) {
  void *ptr = malloc(count);
  if (!ptr)
    throw std::bad_alloc();
  TracyAlloc(ptr, count);
  return ptr;
}

void operator delete(void *ptr) noexcept {
  free(ptr);
  TracyFree(ptr);
}

void operator delete(void *ptr, std::size_t) noexcept {
  free(ptr);
  TracyFree(ptr);
}
#endif

v4dg::Logger v4dg::logger(v4dg::Logger::LogLevel::PrintAlways, v4dg::cerrLogReciever);

void parse_args(int argc, const char *argv[]) {
  argparse::ArgumentParser parser;

  parser.add_argument("-d", "--debug-level")
      .help("set debug level. possible values (d, l, w, e, f, q)")
      .default_value("l").nargs(1);
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

  auto debug_level = parser.get<std::string>("-d");
  if (debug_level.size() != 1)
    throw std::runtime_error(
        std::format("unsupported debug level {}", parser.get<char>("-d")));
  switch (debug_level[0]) {
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

  v4dg::logger.setLogLevel(ll);
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
main([[maybe_unused]] int argc, [[maybe_unused]] const char *argv[]) {
  try {
    std::srand((unsigned int)std::time(NULL));
    parse_args(argc, argv);

    v4dg::logger.Log("starting");
    v4dg::logger.Log("debug level: {}", v4dg::logger.getLogLevel());
    v4dg::logger.Log("path: {}", std::filesystem::current_path().string());

    v4dg::SDL_GlobalContext gc;

    return v4dg::MyGameHandler{}.Run();
  } catch (const std::exception &e) {
    v4dg::logger.FatalError("Exception: {}", e.what());
    return EXIT_FAILURE;
  } catch (...) {
    v4dg::logger.FatalError("Unknown exception");
    return EXIT_FAILURE;
  }
}