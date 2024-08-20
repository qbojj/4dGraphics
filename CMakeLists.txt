cmake_minimum_required(VERSION 3.28)

find_program(CCACHE_PROGRAM ccache)
option(4DG_USE_CCACHE "Use ccache" ON)

if(4DG_USE_CCACHE)
  if (CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
  else()
    message(WARNING "ccache not found")
  endif()
endif()

project(4DG)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(WIN32)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
  option(4DG_BUILD_PARALEL "Build everything in paralel" ON)
  add_compile_options($<$<BOOL:${4DG_BUILD_PARALEL}>:/MP>)
endif()

add_library(WarningSettings INTERFACE)

option(4DG_ALL_WARINGS "Generate all warnings" ON)
if(4DG_ALL_WARINGS)
  if(MSVC)
    target_compile_options(WarningSettings INTERFACE /W4)
  else()
    target_compile_options(
      WarningSettings
      INTERFACE
        -Wall
        -Wextra
        -pedantic
    )
  endif()
endif()

include(CMake/CppModules.cmake)
add_subdirectory(Shaders)
add_subdirectory(4dGraphics)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT
                                                            4dGraphics)
