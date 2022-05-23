cmake_minimum_required (VERSION 3.16)
include(../CMake/CommonMacros.cmake)

project( Collisions )


include_directories( 
	"../shared" 
)

add_library(Collisions 
	"Collisions.cpp"
	"Collisions.h"
)

SETUP_COMMON(Collisions "Collisions" )