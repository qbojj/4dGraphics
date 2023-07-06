cmake_minimum_required (VERSION 3.21)

set(OPTICK_ENABLED OFF CACHE BOOL "" )
set(OPTICK_USE_VULKAN ON CACHE BOOL "")
set(OPTICK_INSTALL_TARGETS OFF CACHE BOOL "" )

add_subdirectory(optick)

set_target_properties(OptickCore PROPERTIES FOLDER "ThirdPartyLibraries")
