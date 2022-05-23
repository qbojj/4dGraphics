#pragma once

#include <string>
#include <type_traits>

std::string GetFileString(const char *pth, bool binary);

// akugb vakye must be POT
template< typename V, typename A = decltype(alignof(std::max_align_t)) >
std::enable_if_t< std::conjunction_v< std::is_integral< V >, std::is_integral< A > >, V >
constexpr AlignOffset( V v, A a = alignof( std::max_align_t ) ) { return (-static_cast<std::make_signed_t<V>>(v)) & static_cast<V>(a - 1); };

// align value must be POT
template< typename V, typename A = decltype(alignof(std::max_align_t)) >
std::enable_if_t< std::conjunction_v< std::is_integral< V >, std::is_integral< A > >, V >
constexpr AlignVal( V v, A a = alignof(std::max_align_t) ) { return (v + static_cast<V>(a - 1)) & -static_cast<std::make_signed_t<V>>(a); };

template< typename V, typename A = decltype(alignof(std::max_align_t)) >
std::enable_if_t< std::conjunction_v< std::is_integral< V >, std::is_integral< A > >, V >
constexpr AlignValNonPOT( V v, A a = alignof(std::max_align_t) ) { return (v + static_cast<V>(a - 1)) / static_cast<V>(a) * static_cast<V>(a); };

inline bool endsWith( const char *s, const char *pattern ) { return (strstr( s, pattern ) - s) == (strlen( s ) - strlen( pattern )); }