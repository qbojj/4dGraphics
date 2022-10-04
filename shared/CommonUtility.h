#pragma once

#include <string>
#include <type_traits>
#include <cstring>
#include <cstddef>

std::string GetFileString(const char *pth, bool binary);

template <typename T>
static inline T AlignUp( T val, T alignment ) { return ( val + alignment - 1 ) & ~( alignment - 1 ); }

template <typename T>
static inline T AlignDown( T val, T alignment ) { return val & ~( alignment - 1 ); }
