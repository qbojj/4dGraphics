﻿# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.16)
project ("4DG")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(BUILD_MP "Build project in parallel" ON)
#option(BUILD_SHARED_LIBS "Build project with shared libs" ON )

if(BUILD_MP)
	set(CMAKE_BUILD_PARALLEL_LEVEL)
endif()

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")

set(ASSIMP_NO_EXPORT ON CACHE BOOL "")
set(ASSIMP_BUILD_DRACO OFF CACHE BOOL "")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "")
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "")

set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "")
set(MESHOPT_BUILD_TOOLS OFF CACHE BOOL "")
set(MESHOPT_BUILD_SHARED_LIBS ON CACHE BOOL "")

set(OPTICK_ENABLED OFF CACHE BOOL "" )

#add_subdirectory(deps/src/etc2comp)

if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

#include_directories(deps/src/vulkan/include)

#add_subdirectory(deps/src/freetype)
add_subdirectory(deps/glfw)
add_subdirectory(deps/optick)
#add_subdirectory(deps/src/easy_sources)
add_subdirectory(deps/assimp)
add_subdirectory(deps/meshoptimizer)
add_subdirectory(deps/volk)

#set(ENABLE_GLSLANG_BINARIES  OFF  CACHE BOOL "")
#set(ENABLE_HLSL              OFF  CACHE BOOL "")
#set(ENABLE_CTEST             OFF  CACHE BOOL "")
#set(ENABLE_OPT               OFF  CACHE BOOL "")
#set(SKIP_GLSLANG_INSTALL     ON   CACHE BOOL "")
#add_subdirectory(deps/src/glslang)

include_directories(.)
include_directories(deps)
#include_directories(deps/src/freetype/include)
#include_directories(deps/glfw/include)

#include_directories(deps/include)
#include_directories(deps/assimp/include)
#include_directories(deps/etc2comp/EtcLib/Etc)
#include_directories(deps/etc2comp/EtcLib/EtcCodec)
#include_directories(deps/glslang/glslang/Include)
#include_directories(deps/taskflow)
#include_directories(deps/rapidjson/include)
#include_directories(${CMAKE_CURRENT_BINARY_DIR}/deps/assimp/include)

#include_directories(deps/glslang)
#include_directories(deps/glslang/StandAlone)
#include_directories(deps/glslang/src)
#include_directories(deps/bullet/src)
#include_directories(deps/ImGuizmo/src)

include_directories(deps/glm)
include_directories(deps/gli)
include_directories(deps/stb)
include_directories(deps/imgui)


add_subdirectory(deps/glad/cmake)
glad_add_library( glad SHARED LANGUAGE C API gl:core=4.6 EXTENSIONS GL_ARB_bindless_texture )

#add_subdirectory(deps/cmake/bullet)
add_subdirectory(deps/cmake/ImGuizmo)


set_property(TARGET glfw          PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET OptickCore    PROPERTY FOLDER "ThirdPartyLibraries")
#set_property(TARGET freetype      PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET glfw          PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET assimp        PROPERTY FOLDER "ThirdPartyLibraries")
#set_property(TARGET EtcLib        PROPERTY FOLDER "ThirdPartyLibraries")
#set_property(TARGET EtcTool       PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET glad          PROPERTY FOLDER "ThirdPartyLibraries")
#set_property(TARGET IrrXML        PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET meshoptimizer PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET volk          PROPERTY FOLDER "ThirdPartyLibraries")
if(WIN32)
	#set_property(TARGET zlib          PROPERTY FOLDER "ThirdPartyLibraries")
	#set_property(TARGET zlibstatic    PROPERTY FOLDER "ThirdPartyLibraries")
	set_property(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTY FOLDER "ThirdPartyLibraries")
endif()
set_property(TARGET uninstall     PROPERTY FOLDER "ThirdPartyLibraries")
#set_property(TARGET glslang       PROPERTY FOLDER "ThirdPartyLibraries/glslang")
#set_property(TARGET OGLCompiler   PROPERTY FOLDER "ThirdPartyLibraries/glslang")
#set_property(TARGET OSDependent   PROPERTY FOLDER "ThirdPartyLibraries/glslang")
#set_property(TARGET SPIRV         PROPERTY FOLDER "ThirdPartyLibraries/glslang")
#set_property(TARGET SPVRemapper   PROPERTY FOLDER "ThirdPartyLibraries/glslang")
#set_property(TARGET GenericCodeGen PROPERTY FOLDER "ThirdPartyLibraries/glslang")
#set_property(TARGET MachineIndependent PROPERTY FOLDER "ThirdPartyLibraries/glslang")


add_subdirectory(shared)
#target_sources(SharedUtils PRIVATE deps/glslang/StandAlone/ResourceLimits.cpp)

add_subdirectory(Collisions)
#add_subdirectory(JJSON)
add_subdirectory(4dGraphics)