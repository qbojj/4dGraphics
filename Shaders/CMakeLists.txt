cmake_minimum_required(VERSION 3.21)
project(Shaders)

# Set the path to the directory containing the shader files
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Find all the GLSL shader files in the directory
file(
  GLOB_RECURSE
  SHADER_FILES
  CONFIGURE_DEPENDS
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

find_program(GLSLC glslc REQUIRE)
set(SHADER_OUTPUTS)

set(SHADER_OUTPUTS_DIR ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Shaders)
set(SHADER_DEPENDS_DIR ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Depends)

# Add a custom target to compile all shaders

# Compile shaders using glslc and generate make dependencies
foreach(SHADER_FILE ${SHADER_FILES})
  file(RELATIVE_PATH SHADER_NAME ${SHADER_DIR} ${SHADER_FILE})
  set(SHADER_OUTPUT ${SHADER_OUTPUTS_DIR}/${SHADER_NAME}.spv)
  set(SHADER_DEPENDENCY ${SHADER_DEPENDS_DIR}/${SHADER_NAME}.d)

  # get dir name of output and dependecy file
  get_filename_component(SHADER_OUTPUT_DIR ${SHADER_OUTPUT} DIRECTORY)
  get_filename_component(SHADER_DEPENDENCY_DIR ${SHADER_DEPENDENCY} DIRECTORY)

  add_custom_command(
    OUTPUT ${SHADER_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR} ${SHADER_DEPENDENCY_DIR}
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

add_custom_target(Shaders ALL DEPENDS ${SHADER_OUTPUTS})

add_custom_command(
  TARGET Shaders
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_OUTPUTS_DIR}
    $<TARGET_FILE_DIR:4dGraphics>/Shaders
  COMMENT "Copying shaders to binary directory")

# make shaders avaiable to other 

install(DIRECTORY ${SHADER_OUTPUTS_DIR} DESTINATION .)
