#pragma once

#include "GlmHeaders.h"

const glm::vec4
	GREY( 192 / 255.f, 192 / 255.f, 192 / 255.f, 1 ),
	DARK_GREY( 0.5, 0.5, 0.5, 1 ),
	VERY_DARK_GREY( 0.25, 0.25, 0.25, 1 ),
	RED( 1, 0, 0, 1 ),
	DARK_RED( 0.5, 0, 0, 1 ),
	VERY_DARK_RED( 0.25, 0, 0, 1 ),
	YELLOW( 1, 1, 0, 1 ),
	DARK_YELLOW( 0.5, 0.5, 0, 1 ),
	VERY_DARK_YELLOW( 0.25, 0.25, 0, 1 ),
	GREEN( 0, 1, 0, 1 ),
	DARK_GREEN( 0, 0.5, 0, 1 ),
	VERY_DARK_GREEN( 0, 0.25, 0, 1 ),
	CYAN( 0, 1, 1, 1 ),
	DARK_CYAN( 0, 0.5, 0.5, 1 ),
	VERY_DARK_CYAN( 0, 0.25, 0.25, 1 ),
	BLUE( 0, 0, 1, 1 ),
	DARK_BLUE( 0, 0, 0.5, 1 ),
	VERY_DARK_BLUE( 0, 0, 0.25, 1 ),
	MAGENTA( 1, 0, 1, 1 ),
	DARK_MAGENTA( 0.5, 0, 0.5, 1 ),
	VERY_DARK_MAGENTA( 0.25, 0, 0.25, 1 ),
	WHITE( 1, 1, 1, 1 ),
	BLACK( 0, 0, 0, 1 ),
	BLANK( 0, 0, 0, 0 );

const glm::vec3
	vUp( 0, 1, 0 ), vDown( 0, -1, 0 ),
	vRight( 1, 0, 0 ), vLeft( -1, 0, 0 ),
	vFront( 0, 0, -1 ), vBack( 0, 0, 1 ),
	vZero( 0, 0, 0 ), vOne( 1, 1, 1 );