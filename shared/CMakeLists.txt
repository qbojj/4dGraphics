cmake_minimum_required(VERSION 3.8)
include(../CMake/CommonMacros.cmake)

project(SharedUtils)

set( SOURCES 
    CommonUtility.h
    GlmHeaders.h
    CommonUtility.cpp
)

find_package( assimp REQUIRED )
#find_package( volk REQUIRED )
find_package( glfw3 REQUIRED )
find_package( gli REQUIRED )
find_package( glm REQUIRED )
find_package( imgui REQUIRED )

find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")


add_library( SharedUtils ${SOURCES} )

target_link_libraries(SharedUtils PUBLIC glad OptickCore glfw assimp::assimp gli glm::glm imgui::imgui ) # glslang SPIRV freetype volk::volk
target_include_directories(SharedUtils PUBLIC ${STB_INCLUDE_DIRS} .)

SETUP_COMMON( SharedUtils "SharedUtils" )