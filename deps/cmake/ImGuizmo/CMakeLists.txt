cmake_minimum_required(VERSION 3.0)

project("ImGuizmo")

include_directories(../../imgui)
include_directories(../../ImGuizmo)

add_library(ImGuizmo ../../ImGuizmo/ImGuizmo.cpp ../../ImGuizmo/ImGuizmo.h)

set_property(TARGET ImGuizmo PROPERTY FOLDER "ThirdPartyLibraries")
#target_link_libraries( ImGuizmo PRIVATE SharedUtils ) # needs linking to ImGui

set_target_properties(ImGuizmo
		PROPERTIES
				CXX_STANDARD 11
				CXX_STANDARD_REQUIRED YES
	)