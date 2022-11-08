#pragma once

#define WIN32_LEAN_AND_MEAN
#define VK_ENABLE_BETA_EXTENSIONS
#define VMA_STATS_STRING_ENABLED 0
#define GLFW_INCLUDE_NONE

#include <volk.h>
#include <glslang/Public/ShaderLang.h>
#include <vk_mem_alloc.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <GLFW/glfw3.h>

#include "optick.h"

#include "GlmHeaders.h"

#include "Collisions.h"

#include <thread>
#include <string>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <set>
#include <regex>
#include <bitset>
#include <numeric>

#include <stdlib.h>
#include <math.h>

#include <stdarg.h>
