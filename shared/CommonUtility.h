#pragma once

#include <string>
#include <type_traits>
#include <cstring>
#include <cstddef>

std::string GetFileString(const char *pth, bool binary);

template <typename T, typename V>
static inline T AlignUp( T val, V alignment ) 
{ return ( val + static_cast<T>(alignment) - 1 ) & ~( static_cast<T>( alignment ) - 1 ); }

template <typename T, typename V>
static inline T AlignDown( T val, V alignment ) 
{ return val & ~( static_cast<T>(alignment) - 1 ); }
