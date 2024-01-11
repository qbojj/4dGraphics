cmake_minimum_required(VERSION 3.12)

macro(SETUP_COMMON projectname dirname )
	if(MSVC)
		set_property(TARGET ${projectname} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
		target_compile_options(${projectname} PRIVATE /permissive- /Zc:inline /Zc:lambda /Zc:preprocessor /Zc:throwingNew ) # add all conformance options on msvc compiler
	endif()

	target_compile_features( ${projectname} PRIVATE cxx_std_23 )
	set_target_properties( ${projectname} PROPERTIES 
		VISIBILITY_INLINES_HIDDEN true
		CXX_VISIBILITY_PRESET hidden
	)
endmacro()