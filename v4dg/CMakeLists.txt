include(../CMake/CommonMacros.cmake)
project(v4dg)

find_package(argparse CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(Taskflow CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator-hpp CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(unordered_dense CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)

file(GLOB SOURCES CONFIGURE_DEPENDS *.cpp)
file(GLOB HEADERS CONFIGURE_DEPENDS *.hpp *.h)
file(GLOB MODULE_FILES CONFIGURE_DEPENDS *.cppm)

add_library(v4dg)
target_sources(
  v4dg
  PRIVATE  ${SOURCES}
  PUBLIC FILE_SET HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES
         ${HEADERS}
         )
target_compile_features(v4dg PRIVATE cxx_std_23)
target_include_directories(v4dg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(
  v4dg
  PUBLIC
  <taskflow/algorithm/critical.hpp>
  <taskflow/algorithm/for_each.hpp>
  <taskflow/taskflow.hpp>
  <vulkan/vulkan.hpp>
  <vulkan/vulkan_extension_inspection.hpp>
  <vulkan/vulkan_format_traits.hpp>
  <vulkan/vulkan_hash.hpp>
  <vulkan/vulkan_raii.hpp>
  <vulkan/vulkan_to_string.hpp>
  <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
  <glm/glm.hpp>
  <glm/ext.hpp>
  <SDL2/SDL.h>
  <tracy/Tracy.hpp>
  <imgui.h>
  <imgui_impl_sdl2.h>
  <imgui_impl_vulkan.h>
  # all of c++23 headers
  <algorithm>
  <any>
  <array>
  <atomic>
  <barrier>
  <bit>
  <bitset>
  <charconv>
  <chrono>
  <codecvt>
  <compare>
  <complex>
  <concepts>
  <condition_variable>
  <coroutine>
  <deque>
  <exception>
  # <execution>
  <expected>
  <filesystem>
  # <flat_map> <flat_set>
  <format>
  <forward_list>
  <fstream>
  <functional>
  <future>
  <generator>
  <initializer_list>
  <iomanip>
  <ios>
  <iosfwd>
  <iostream>
  <istream>
  <iterator>
  <latch>
  <limits>
  <list>
  <locale>
  <map>
  # <mdspan>
  <memory>
  <memory_resource>
  <mutex>
  <new>
  <numbers>
  <numeric>
  <optional>
  <ostream>
  <print>
  <queue>
  <random>
  <ranges>
  <ratio>
  <regex>
  <scoped_allocator>
  <semaphore>
  <set>
  <shared_mutex>
  <source_location>
  <span>
  <spanstream>
  <sstream>
  <stack>
  <stacktrace>
  <stdexcept>
  <stdfloat>
  <stop_token>
  <streambuf>
  <string>
  <string_view>
  <syncstream>
  <system_error>
  <thread>
  <tuple>
  <type_traits>
  <typeindex>
  <typeinfo>
  <unordered_map>
  <unordered_set>
  <utility>
  <valarray>
  <variant>
  <vector>
  <version>)

target_link_libraries(
  v4dg
  PUBLIC imgui::imgui
         $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
         Taskflow::Taskflow
         glm-module
         vulkan-hpp-module
         unordered_dense::unordered_dense
         unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
         Tracy::TracyClient
         WarningSettings argparse::argparse fastgltf::fastgltf
         KTX::ktx KTX::astcenc-avx2-static)

target_include_directories(v4dg PUBLIC ${PROJECT_SOURCE_DIR}
                                             ${Stb_INCLUDE_DIR})

target_compile_definitions(
  v4dg
  PRIVATE WIN32_LEAN_AND_MEAN TRACY_VK_USE_SYMBOL_TABLE=1)

setup_common(v4dg "v4dg")

install(TARGETS v4dg)
