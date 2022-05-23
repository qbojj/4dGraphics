cmake_minimum_required(VERSION 3.12)

macro(SETUP_COMMON projectname dirname )
	# On Linux/macOS the binaries go to <root>/bin folder
	if(UNIX)
		set_target_properties(${projectname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
	elseif(MSVC)
		set_property(TARGET ${projectname} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data/")
		#set_property(TARGET ${projectname} APPEND PROPERTY CXX_FLAGS "/Zc:__cplusplus /std:c17")
	endif()

	set_target_properties( ${projectname}
		PROPERTIES
				# FOLDER ${dirname}
				CXX_STANDARD 20
				CXX_STANDARD_REQUIRED YES
	)

endmacro()
