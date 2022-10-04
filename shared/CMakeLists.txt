cmake_minimum_required(VERSION 3.8)
include(../CMake/CommonMacros.cmake)

project(SharedUtils)

set( SOURCES 
    CommonUtility.h
    CommonUtility.cpp
)

find_package( Threads )
find_package( assimp REQUIRED )
find_package( volk REQUIRED )
find_package( glfw3 REQUIRED )
find_package( glm REQUIRED )
find_package( imgui REQUIRED )
find_package( glslang REQUIRED )
find_package( unofficial-portable-snippets REQUIRED )
find_package( unofficial-vulkan-memory-allocator REQUIRED )
    

find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")

add_library( GlmHeaders INTERFACE )
target_include_directories( GlmHeaders INTERFACE ${PROJECT_SOURCE_DIR}/GlmHeaders/ )
target_link_libraries( GlmHeaders INTERFACE glm::glm )

add_library( SharedUtils ${SOURCES} )

target_link_libraries(SharedUtils PUBLIC 
    OptickCore GlmHeaders
    glfw 
    assimp::assimp 
    imgui::imgui 
    volk::volk 
    unofficial::portable-snippets::debug-trap unofficial::portable-snippets::builtin
    unofficial::vulkan-memory-allocator::vulkan-memory-allocator
    glslang::glslang glslang::SPIRV
) # freetype
target_include_directories(SharedUtils PUBLIC ${STB_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR})

SETUP_COMMON( SharedUtils "SharedUtils" )