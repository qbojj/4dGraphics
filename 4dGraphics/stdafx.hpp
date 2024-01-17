#pragma once

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/critical.hpp>
#include <taskflow/algorithm/for_each.hpp>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_extension_inspection.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_hash.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include <tracy/Tracy.hpp>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <atomic>
#include <bitset>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <math.h>
#include <mutex>
#include <numeric>
#include <regex>
#include <set>
#include <cstdarg>
#include <stdexcept>
#include <cstdlib>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>