#ifndef GLSL_INIT_GLSL_
#define GLSL_INIT_GLSL_

#if __VERSION__ < 140 // gl 3.1
#	extension GL_EXT_texture_array : require
#	extension GL_ARB_uniform_buffer_object : require
#endif

#if __VERSION__ < 400
#	extension GL_ARB_texture_cube_map_array : require
#endif

#if __VERSION__ < 420
#	extension GL_ARB_shading_language_420pack : require
#	extension GL_ARB_shader_image_load_store : require
#endif

#if __VERSION__ < 430
#	extension GL_ARB_shader_storage_buffer_object : require
#endif

#ifdef FOR_RENDERDOC
#	if !FOR_RENDERDOC
#		extension GL_ARB_bindless_texture : enable
#	endif
#endif

#extension GL_ARB_shader_draw_parameters : require

#endif // GLSL_INIT_GLSL_