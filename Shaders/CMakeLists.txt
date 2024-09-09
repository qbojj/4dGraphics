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

set(OPTS_RELEASE -O)
set(OPTS_REL_WITH_DEB_INFO -g -O)
set(OPTS_MIN_SIZE_REL -Os)
set(OPTS_DEBUG -g -O0 -DBINDLESS_CHECKS -DBINDLESS_CHECK_BUFFER)

set(OPTS
    --target-env=vulkan1.3
    $<$<CONFIG:Release>:${OPTS_RELEASE}>
    $<$<CONFIG:RelWithDebInfo>:${OPTS_REL_WITH_DEB_INFO}>
    $<$<CONFIG:MinSizeRel>:${OPTS_MIN_SIZE_REL}>
    $<$<CONFIG:Debug>:${OPTS_DEBUG}>)

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
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
            ${SHADER_DEPENDENCY_DIR}
    COMMAND ${GLSLC} -o "${SHADER_OUTPUT}" -MD -MF "${SHADER_DEPENDENCY}"
            "${OPTS}" "${SHADER_FILE}"
    COMMAND_EXPAND_LISTS
    DEPENDS ${SHADER_FILE}
    DEPFILE ${SHADER_DEPENDENCY}
    COMMENT "Compiling shader ${SHADER_FILE}")

  list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

# Add a custom target to compile all shaders
add_custom_target(Shaders ALL DEPENDS ${SHADER_OUTPUTS})

include(GNUInstallDirs)
install(DIRECTORY ${SHADER_OUTPUTS_DIR} DESTINATION ${CMAKE_INSTALL_DATADIR}/4dGraphics)
