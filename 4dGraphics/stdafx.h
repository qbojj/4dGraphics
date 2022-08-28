#pragma once

#define WIN32_LEAN_AND_MEAN

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>

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

#include <stb_rect_pack.h>
#include <stb_image.h>
#include <stb_sprintf.h>

#define DATA_PATH "data/"
#define MODELS_BASE_PATH DATA_PATH "3dModels/"
#define FONTS_BASE_PATH DATA_PATH "fonts/"
#define SHADER_BASE_PTH "Shaders/"
