#pragma once

#define WIN32_LEAN_AND_MEAN

#include <volk.h>
#include <glslang/Public/ShaderLang.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#undef APIENTRY

#include "optick.h"

#include "GlmHeaders.h"

#include "Collisions.h"
using namespace collisions;

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
