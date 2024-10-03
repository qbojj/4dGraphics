#include "Config.hpp"
#include "v4dgCore.hpp"

#include <SDL2/SDL_error.h>
#include <SDL2/SDL_filesystem.h>

#include <filesystem>

using namespace v4dg;

Config::Config(zstring_view app_name) {
  namespace fs = std::filesystem;

  fs::path base_path;
  if (char *base_path_c = SDL_GetBasePath()) {
    base_path = base_path_c;
    SDL_free(base_path_c);
  } else {
    base_path = fs::current_path();
  }

  if (char *pref_path_c = SDL_GetPrefPath(nullptr, app_name.data())) {
    user_data_dir_ = pref_path_c;
    SDL_free(pref_path_c);
  } else {
    throw exception("could not get preferred path: {}", SDL_GetError());
  }

  cache_dir_ = user_data_dir_ / "cache";

  std::string_view app_name_sv{app_name};

  // remove trailing slashes
  while (!base_path.has_filename() && base_path.has_parent_path()) {
    base_path = base_path.parent_path();
  }

  if (base_path.filename() == "bin") {
    data_dir_ = base_path.parent_path() / "share" / app_name_sv;
  } else {
    data_dir_ = base_path;
  }

  // if we know something more, change the paths

#ifdef __linux__
  if (const char *home_c = std::getenv("HOME")) {
    cache_dir_ = fs::path{home_c} / ".cache" / app_name_sv;
  }
#elifdef _WIN32
  if (const char *temp_user_data_c = std::getenv("LOCALAPPDATA")) {
    cache_dir_ = fs::path{temp_user_data_c} / app_name_sv;
  }
#endif

  fs::create_directories(cache_dir_);
  fs::create_directories(user_data_dir_);
}
