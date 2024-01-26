cmake_minimum_required(VERSION 3.21)
project(Shaders)

# Set the path to the directory containing the shader files
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Find all the GLSL shader files in the directory
file(
  GLOB_RECURSE
  SHADER_FILES
  ${SHADER_DIR}/*.vert
  ${SHADER_DIR}/*.tesc
  ${SHADER_DIR}/*.tese
  ${SHADER_DIR}/*.geom
  ${SHADER_DIR}/*.frag
  ${SHADER_DIR}/*.comp
  ${SHADER_DIR}/*.rgen
  ${SHADER_DIR}/*.rint
  ${SHADER_DIR}/*.rahit
  ${SHADER_DIR}/*.rchit
  ${SHADER_DIR}/*.rmiss
  ${SHADER_DIR}/*.rcall
  ${SHADER_DIR}/*.task
  ${SHADER_DIR}/*.mesh)

find_program(GLSLC glslc)
set(SHADER_OUTPUTS)

# Compile shaders using glslc and generate make dependencies
foreach(SHADER_FILE ${SHADER_FILES})
  file(RELATIVE_PATH SHADER_NAME ${SHADER_DIR} ${SHADER_FILE})
  set(SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${SHADER_NAME}.spv)
  set(SHADER_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${SHADER_NAME}.d)

  add_custom_command(
    OUTPUT ${SHADER_OUTPUT}
    COMMAND
      ${GLSLC} -o ${SHADER_OUTPUT} -MD -MF ${SHADER_DEPENDENCY}
      --target-env=vulkan1.3 $<$<CONFIG:Debug,RelWithDebInfo>:-g>
      $<$<CONFIG:Release,RelWithDebInfo>:-O> $<$<CONFIG:MinSizeRel>:-Os>
      $<$<CONFIG:Debug>:-O0> ${SHADER_FILE}
    DEPENDS ${SHADER_FILE}
    DEPFILE ${SHADER_DEPENDENCY}
    COMMENT "Compiling shader ${SHADER_FILE}")

  list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

# Add a custom target to compile all shaders
add_custom_target(Shaders ALL DEPENDS ${SHADER_OUTPUTS})

install(FILES ${SHADER_OUTPUTS} DESTINATION share/shaders)
