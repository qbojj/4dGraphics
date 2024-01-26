#include "cppHelpers.hpp"
#include "v4dgCore.hpp"

#include <cstring>
#include <exception>
#include <fstream>
#include <system_error>

namespace v4dg::detail {
std::optional<std::string> GetStreamString(std::istream &file, bool binary) {
  if (!file)
    return std::nullopt;
  
  file.seekg(0, file.end);
  auto length = file.tellg();
  file.seekg(0, file.beg);

  if (!binary && length >= 3) {
    // text mode -> try removing UTF-8 BOM
    static constexpr unsigned char BOM[] = {0xEF, 0xBB, 0xBF};
    char buf[3];
    file.read(buf, 3);

    if (memcmp(buf, BOM, 3) == 0) {
      // BOM detected
      length -= 3;
    } else {
      // no BOM detected -> revert changes
      file.seekg(0, file.beg);
    }
  }

  std::string s;
  s.resize_and_overwrite(length, [&](char *buf, std::size_t len) {
    file.read(buf, len);
    return file.gcount();
  });

  return s;
}
} // namespace v4dg::detail