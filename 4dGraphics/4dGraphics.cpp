#include "Config.hpp"
#include "Debug.hpp"
#include "GameCore.hpp"
#include "GameHandler.hpp"
#include "ILogReciever.hpp"
#include "cppHelpers.hpp"

#include <SDL_config.h>
#include <argparse/argparse.hpp>
#include <tracy/Tracy.hpp>

#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <iterator>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb_image.h>

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

namespace {
void parse_args(std::span<const char *> args) {
  argparse::ArgumentParser parser;

  parser.add_argument("-d", "--debug-level")
      .help("set debug level. possible values (d, l, w, e, f, q)")
      .default_value("l")
      .nargs(1);
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
    parser.parse_args(static_cast<int>(args.size()), args.data());
  } catch (const std::exception &e) {
    std::cout << e.what() << ' ' << parser;
    throw;
  }

  v4dg::Logger::LogLevel log_level{};

  auto debug_level = parser.get<std::string>("-d");
  if (debug_level.size() != 1) {
    throw std::runtime_error(
        std::format("unsupported debug level {}", parser.get<char>("-d")));
  }
  switch (debug_level[0]) {
    using enum v4dg::Logger::LogLevel;
  case 'd':
    log_level = Debug;
    break;
  case 'l':
    log_level = Log;
    break;
  case 'w':
    log_level = Warning;
    break;
  case 'e':
    log_level = Error;
    break;
  case 'f':
    log_level = FatalError;
    break;
  case 'q':
    log_level = PrintAlways;
    break;
  default:
    throw std::runtime_error(
        std::format("unsupported debug level {}", parser.get<char>("-d")));
  }

  using sp_lr = std::shared_ptr<v4dg::ILogReciever>;
  std::vector<sp_lr> recievers;

  recievers.push_back(std::make_shared<v4dg::TracyLogReciever>());

  if (!parser.get<bool>("-q")) {
    recievers.push_back(std::make_shared<v4dg::CerrLogReciever>());
  }

  if (parser.is_used("--log-path")) {
    recievers.push_back(std::make_shared<v4dg::FileLogReciever>(
        parser.get<std::string>("--log-path")));
  }

#ifdef _WIN32
  if (parser.get<bool>("--output-debug-string"))
    recievers.push_back(std::make_shared<v4dg::DebugOutputLogReciever>());

  if (parser.get<bool>("--message-box"))
    recievers.push_back(std::make_shared<v4dg::MessageBoxLogReciever>());
#endif

  v4dg::logger.setLogLevel(log_level);
  v4dg::logger.setLogReciever(v4dg::MultiLogReciever::from_span(recievers));
}
} // namespace

extern "C" int main(int argc, const char *argv[]) try {
  std::span const args{argv, static_cast<std::size_t>(argc)};
  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  parse_args(args);

  v4dg::logger.Log("starting");
  v4dg::logger.Log("debug level: {}", v4dg::logger.getLogLevel());
  v4dg::logger.Log("path: {}", std::filesystem::current_path().string());

  const v4dg::SDL_GlobalContext _{};

  v4dg::Config cfg{"4dGraphics"};

  v4dg::logger.Log("data dir: {}\ncache dir: {}\nuser data dir: {}",
                   cfg.data_dir().string(), cfg.cache_dir().string(),
                   cfg.user_data_dir().string());

  return v4dg::MyGameHandler{cfg}.Run();
} catch (const std::exception &e) {
  v4dg::logger.FatalError("Exception: {}", e.what());
  return EXIT_FAILURE;
} catch (...) {
  v4dg::logger.FatalError("Unknown exception");
  return EXIT_FAILURE;
}
