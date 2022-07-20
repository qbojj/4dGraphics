cmake_minimum_required (VERSION 3.21)
include(../CMake/CommonMacros.cmake)

project(4dGraphics)

# get by ls -1
set( 4DGSources
	"4dGraphics.cpp"
	"BufferAlloc.cpp"
	"BufferAlloc.h"
	"CMakeLists.txt"
	"Debug.cpp"
	"Debug.h"
	"GLId.cpp"
	"GLId.h"
	"GLUniformBlockDefinitions.h"
	"GameCore.cpp"
	"GameCore.h"
	"GameInputHandler.cpp"
	"GameInputHandler.h"
	"GameRenderHandler.cpp"
	"GameRenderHandler.h"
	"GameTickHandler.cpp"
	"GameTickHandler.h"
	"MeshExecution.cpp"
	"MeshExecution.h"
	"Objects.cpp"
	"Objects.h"
	"Shader.cpp"
	"Shader.h"
	"ShaderUniforms.h"
	"TextureLoad.cpp"
	"TextureLoad.h"
	"stdafx.cpp"
	"stdafx.h"
	"CommandQueue.h"
	"GameObject.h"
	"ImguiBufferedHelper.cpp"
	"ImguiBufferedHelper.h"
)

add_executable( 4dGraphics  ${4DGSources} )
target_include_directories( 4dGraphics PUBLIC ${PROJECT_SOURCE_DIR} PRIVATE $<PATH:NORMAL_PATH,${PROJECT_SOURCE_DIR}/../Shaders/include> )

target_precompile_headers( 4dGraphics PRIVATE "stdafx.h" )

target_link_libraries( 4dGraphics PRIVATE Collisions SharedUtils )

if(WIN32)
	target_compile_definitions( 4dGraphics PRIVATE REQUIRE_WINMAIN )
	set_property( TARGET 4dGraphics PROPERTY WIN32_EXECUTABLE true )
endif()

SETUP_COMMON(4dGraphics "4dGraphics" )

add_custom_command( 
	TARGET 4dGraphics POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_RUNTIME_DLLS:4dGraphics> $<TARGET_FILE:4dGraphics> # $<TARGET_FILE:4dGraphics> is added to never have empty set to copy
		$<TARGET_FILE_DIR:4dGraphics>
	COMMAND_EXPAND_LISTS
)