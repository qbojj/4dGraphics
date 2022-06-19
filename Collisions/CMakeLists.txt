cmake_minimum_required (VERSION 3.16)
include(../CMake/CommonMacros.cmake)

project( Collisions )

add_library(Collisions 
	"Collisions.cpp"
	"Collisions.h"
)

target_link_libraries(Collisions PRIVATE SharedUtils)
target_include_directories(Collisions PUBLIC .)

SETUP_COMMON(Collisions "Collisions" )