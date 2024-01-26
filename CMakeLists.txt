cmake_minimum_required(VERSION 3.21)

set(CMAKE_MACOSX_RPATH True)

if(APPLE)
  list(APPEND CMAKE_INSTALL_RPATH "@executable_path/../lib")
else()
  list(APPEND CMAKE_INSTALL_RPATH "${ORIGIN}/../lib")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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
        $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-Wno-gnu-zero-variadic-macro-arguments>
    )
  endif()
endif()

add_subdirectory(Shaders)
add_subdirectory(4dGraphics)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT
                                                            4dGraphics)