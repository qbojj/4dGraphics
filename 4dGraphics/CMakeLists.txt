include(../CMake/CommonMacros.cmake)
project(4dGraphics)

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

# create a vulkan-hpp cpp20 module target
add_library(vulkan-hpp-module INTERFACE)#)
# target_sources(vulkan-hpp-module PUBLIC
#  FILE_SET CXX_MODULES
#  BASE_DIRS ${Vulkan_INCLUDE_DIR}/vulkan
#  FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm
#)
target_compile_features(vulkan-hpp-module INTERFACE cxx_std_23)
target_link_libraries(vulkan-hpp-module INTERFACE Vulkan::Headers)
target_compile_definitions(vulkan-hpp-module
INTERFACE
  VK_ENABLE_BETA_EXTENSIONS=1
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
  VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=0
)

file(GLOB SOURCES CONFIGURE_DEPENDS *.cpp)
file(GLOB HEADERS CONFIGURE_DEPENDS *.hpp *.h)

add_executable(4dGraphics WIN32)
target_sources(
  4dGraphics
  PRIVATE  ${SOURCES}
  PUBLIC FILE_SET HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES
         ${HEADERS})
target_compile_features(4dGraphics PRIVATE cxx_std_23)

target_precompile_headers(
  4dGraphics
  PRIVATE
  <glm/ext.hpp>
  <glm/glm.hpp>
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
  <execution>
  <expected>
  <filesystem>
  # <flat_map> <flat_set>
  <format>
  <forward_list>
  <fstream>
  <functional>
  <future>
  # <generator>
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
  # <print>
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
  4dGraphics
  PUBLIC imgui::imgui
         $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
         $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
         Taskflow::Taskflow
         glm::glm
         Vulkan::Headers
         unordered_dense::unordered_dense
         unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
         Tracy::TracyClient
  PRIVATE WarningSettings argparse::argparse fastgltf::fastgltf
          KTX::ktx KTX::astcenc-avx2-static)

target_include_directories(4dGraphics PUBLIC ${PROJECT_SOURCE_DIR}
                                             ${Stb_INCLUDE_DIR})

target_compile_definitions(
  4dGraphics
  PUBLIC GLM_FORCE_SILENT_WARNINGS
         GLM_FORCE_DEPTH_ZERO_TO_ONE
         GLM_FORCE_SWIZZLE
         GLM_FORCE_RADIANS
         GLM_FORCE_CTOR_INIT
         GLM_ENABLE_EXPERIMENTAL
         VK_ENABLE_BETA_EXTENSIONS=1
         VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
         VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=0
  PRIVATE WIN32_LEAN_AND_MEAN TRACY_VK_USE_SYMBOL_TABLE=1)

setup_common(4dGraphics "4dGraphics")

add_custom_command(
  TARGET 4dGraphics
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:4dGraphics>
    $<TARGET_FILE:4dGraphics>
    # $<TARGET_FILE:4dGraphics> is added to never have an empty set to copy
    $<TARGET_FILE_DIR:4dGraphics>
  COMMAND_EXPAND_LISTS)

install(TARGETS 4dGraphics)
