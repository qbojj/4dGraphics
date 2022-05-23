cmake_minimum_required(VERSION 3.8)
include(../CMake/CommonMacros.cmake)

project(SharedUtils)

file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c??)
file(GLOB_RECURSE HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

add_library(SharedUtils ${SRC_FILES} ${HEADER_FILES} )
target_link_libraries(SharedUtils PUBLIC glad glfw volk assimp OptickCore ) # glslang SPIRV freetype

SETUP_COMMON( SharedUtils "SharedUtils" )