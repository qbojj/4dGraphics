#pragma once

#define WIN32_LEAN_AND_MEAN

//#pragma warning(push)
//#pragma warning( disable: 4251 )
//#include <glbinding/glbinding.h>
//#include <glbinding-aux/ContextInfo.h>
//#include <glbinding-aux/debug.h>
//#include <glbinding-aux/logging.h>
//#pragma warning(pop)

//#define ASSIMP_DLL
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>
//#pragma comment(lib, "assimp.lib")

#include <ft2build.h>
#include FT_FREETYPE_H
//#pragma comment(lib, "freetype.lib")

//#define GLFW_DLL
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
//#pragma comment(lib, "glfw3dll.lib")
#undef APIENTRY

#include "optick.h"

#include "GlmHeaders.h"

#include "Collisions.h"
using namespace collisions;

//#include <robin_hood.h>

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
#include <execution>
#include <regex>
// #include <compare>
#include <bitset>

#include <stdlib.h>
#include <math.h>

#include <stdarg.h>

#include <stb_rect_pack.h>
#include <stb_image.h>
#include <stb_sprintf.h>
