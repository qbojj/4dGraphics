cmake_minimum_required(VERSION 3.28)

find_package(glm CONFIG REQUIRED)
find_package(VulkanHeaders REQUIRED)

add_library(vulkan-hpp-module INTERFACE)
# target_sources(vulkan-hpp-module PUBLIC
#   FILE_SET CXX_MODULES
#   BASE_DIRS ${Vulkan_INCLUDE_DIR}/vulkan
#   FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm
# )
target_compile_features(vulkan-hpp-module INTERFACE cxx_std_23)
target_link_libraries(vulkan-hpp-module INTERFACE Vulkan::Headers)
target_compile_definitions(vulkan-hpp-module
INTERFACE
  # VK_ENABLE_BETA_EXTENSIONS=1
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
  VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=0
)

add_library(glm-module INTERFACE)
# target_sources(glm-module PUBLIC
#   FILE_SET CXX_MODULES
#   BASE_DIRS ${Vulkan_INCLUDE_DIR}/glm
#   FILES ${Vulkan_INCLUDE_DIR}/glm/glm.cppm
# )
target_compile_features(glm-module INTERFACE cxx_std_23)
target_link_libraries(glm-module INTERFACE glm::glm)
target_compile_definitions(glm-module INTERFACE
  GLM_FORCE_SILENT_WARNINGS
  GLM_FORCE_DEPTH_ZERO_TO_ONE
#  GLM_FORCE_SWIZZLE
  GLM_FORCE_RADIANS
#  GLM_FORCE_CTOR_INIT
#  GLM_ENABLE_EXPERIMENTAL
)
