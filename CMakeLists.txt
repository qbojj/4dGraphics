cmake_minimum_required (VERSION 3.21)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
set(CMAKE_CONFIGURATION_TYPES "Release;Debug;RelWithDebInfo;MinSizeRel" CACHE STRING "Configuration types for multi-configuration buildsystems")
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build configuration for single-configuration buildsystems")

set(CMAKE_MACOSX_RPATH True)

if( APPLE )
	list(APPEND CMAKE_INSTALL_RPATH "@executable_path/../lib" )
else()
	list(APPEND CMAKE_INSTALL_RPATH "${ORIGIN}/../lib" )
endif()

project( 4DG )

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if( WIN32 )
	add_compile_definitions( _CRT_SECURE_NO_WARNINGS )
endif()

if( MSVC )
	option( 4DG_BUILD_PARALEL "Build everything in paralel" ON )
	add_compile_options( $<$<BOOL:${4DG_BUILD_PARALEL}>:/MP> )
endif()

option( 4DG_ALL_WARINGS "Generate all warnings" ON)
if( 4DG_ALL_WARINGS )
	if (MSVC)
		add_compile_options( /W4 )
	else()
		add_compile_options( -Wall -Wextra -pedantic $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-Wno-gnu-zero-variadic-macro-arguments> )
	endif()
endif()

set(OPTICK_ENABLED OFF CACHE BOOL "" )
set(OPTICK_INSTALL_TARGETS OFF CACHE BOOL "" )

add_subdirectory(deps/optick)

set_property(TARGET OptickCore    PROPERTY FOLDER "ThirdPartyLibraries")

add_subdirectory(shared)
add_subdirectory(Collisions)
add_subdirectory(4dGraphics)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT 4dGraphics)

install( TARGETS 4dGraphics SharedUtils Collisions )
include(InstallRequiredSystemLibraries)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/CMake/FixBundle.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/FixBundle.cmake
  @ONLY
  )

install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/FixBundle.cmake)