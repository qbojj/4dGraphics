#pragma once

#include "cppHelpers.hpp"

#include <filesystem>

namespace v4dg {
struct Config {
public:
  Config(zstring_view app_name);

  [[nodiscard]] auto &data_dir() const noexcept { return data_dir_; }
  [[nodiscard]] auto &cache_dir() const noexcept { return cache_dir_; }
  [[nodiscard]] auto &user_data_dir() const noexcept { return user_data_dir_; }

private:
  std::filesystem::path data_dir_;      // like /usr/share/<app>
  std::filesystem::path cache_dir_;     // like ~/.cache/<app>
  std::filesystem::path user_data_dir_; // like ~/.local/share/<app>
};
} // namespace v4dg
