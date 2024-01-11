#include "cppHelpers.hpp"
#include "v4dgCore.hpp"

#include <cstring>
#include <exception>
#include <fstream>
#include <system_error>

std::string GetFileString(std::string_view pth, bool binary) {
  if (pth.data()[pth.size()] != '\0')
    throw std::invalid_argument("path must be null-terminated");
  
  std::ifstream file;
  using ib = std::ios_base;

  file.exceptions(ib::failbit | ib::badbit);
  file.open(pth.data(), binary ? ib::in | ib::binary : ib::in);

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

  std::string buffer(length, '\0');
  file.read(buffer.data(), length);
  return buffer;
}