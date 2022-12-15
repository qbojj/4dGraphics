cmake_minimum_required (VERSION 3.21)
include(../CMake/CommonMacros.cmake)

project(4dGraphics)

# get by ls -1
set( 4DGSources
	"4dGraphics.cpp"
	"Debug.cpp"
	"Debug.h"
	"GameCore.cpp"
	"GameCore.h"
	"GameHandler.cpp"
	"GameHandler.h"
	"stdafx.cpp"
	"stdafx.h"
	"ImguiBufferedHelper.cpp"
	"ImguiBufferedHelper.h"
	"Shader.cpp"
	"Shader.h"
	"VulkanHelpers.h"
	"VulkanHelpers.cpp"
	"Constants.h"
	"RenderGraph.h"
	"RenderGraph.cpp"
	"AutoDestructor.h"
)

add_executable( 4dGraphics WIN32 ${4DGSources} )
target_include_directories( 4dGraphics PUBLIC ${PROJECT_SOURCE_DIR} PRIVATE ${PROJECT_SOURCE_DIR}/../Shaders/include )

target_precompile_headers( 4dGraphics PRIVATE "stdafx.h" )

target_link_libraries( 4dGraphics PRIVATE Collisions SharedUtils )

target_compile_definitions( 4dGraphics PRIVATE $<$<BOOL:${WIN32}>:REQUIRE_WINMAIN> )

SETUP_COMMON(4dGraphics "4dGraphics" )

add_custom_command( 
	TARGET 4dGraphics POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_RUNTIME_DLLS:4dGraphics> $<TARGET_FILE:4dGraphics> # $<TARGET_FILE:4dGraphics> is added to never have empty set to copy
		$<TARGET_FILE_DIR:4dGraphics>
	COMMAND_EXPAND_LISTS
)