// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

// Some C++23 implementations report a provisional date; C++20 remains unsupported.
// MSVC's _MSVC_LANG reflects the selected mode without requiring /Zc:__cplusplus.
#if defined(_MSVC_LANG)
#if _MSVC_LANG <= 202002L
#error "Serializer requires C++23 or later"
#endif
#elif __cplusplus <= 202002L
#error "Serializer requires C++23 or later"
#endif

#if !defined(__cpp_if_consteval) || __cpp_if_consteval < 202106L
#error "Serializer requires C++23 if consteval support"
#endif

#include <version>

// Test library capabilities independently of the compiler's selected language mode.
#if !defined(__cpp_lib_byteswap) || __cpp_lib_byteswap < 202110L
#error "Serializer requires a C++23 standard library with std::byteswap"
#endif
#if !defined(__cpp_lib_span) || __cpp_lib_span < 202002L
#error "Serializer requires a standard library with std::span"
#endif
