cmake_minimum_required (VERSION 3.16)
include(../CMake/CommonMacros.cmake)

project(4dGraphics)


include_directories(
	"../shared" 
	"../Collisions"
	"../data/shaders/include"
)

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
)

if(WIN32)
	set( 4DGSources
		${4DGSources}
		"windows/StackWalker.cpp"
		"windows/StackWalker.h"
	)
	include_directories( "./windows" )
endif()

add_executable( 4dGraphics  ${4DGSources} )
target_precompile_headers( 4dGraphics PRIVATE "stdafx.h" )

if( UNIX AND NOT APPLE )
	find_package(TBB REQUIRED)
	target_link_libraries(4dGraphics PRIVATE TBB::tbb)
endif()

target_link_libraries( 4dGraphics PRIVATE Collisions SharedUtils )

if(WIN32)
	add_definitions( -DREQUIRE_WINMAIN )
	set_property( TARGET 4dGraphics PROPERTY WIN32_EXECUTABLE true )
endif()
SETUP_COMMON(4dGraphics "4dGraphics" )