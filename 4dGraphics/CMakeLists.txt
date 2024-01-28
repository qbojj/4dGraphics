cmake_minimum_required(VERSION 3.21)
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

file(GLOB SOURCES *.cpp)
file(GLOB HEADERS *.hpp *.h)

add_executable(4dGraphics WIN32)
target_sources(4dGraphics
  PRIVATE
    ${SOURCES}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${HEADERS}
  )

target_precompile_headers(4dGraphics PRIVATE stdafx.hpp)

target_link_libraries(
  4dGraphics
  PRIVATE WarningSettings
          argparse::argparse
          fastgltf::fastgltf
          glm::glm
          Vulkan::Headers
          imgui::imgui
          $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
          $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
          Taskflow::Taskflow
          Tracy::TracyClient
          unordered_dense::unordered_dense
          unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp)

target_include_directories(
  4dGraphics
  PUBLIC ${PROJECT_SOURCE_DIR} ${Stb_INCLUDE_DIR})

target_compile_definitions(
  4dGraphics
  PRIVATE WIN32_LEAN_AND_MEAN
          VK_ENABLE_BETA_EXTENSIONS=1
          VK_NO_PROTOTYPES=1
          VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=0
          # TRACY_ENABLE=1
          GLM_FORCE_SILENT_WARNINGS
          GLM_FORCE_DEPTH_ZERO_TO_ONE
          GLM_FORCE_SWIZZLE
          GLM_FORCE_RADIANS
          GLM_FORCE_CTOR_INIT
          GLM_ENABLE_EXPERIMENTAL
          TRACY_VK_USE_SYMBOL_TABLE=1)

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