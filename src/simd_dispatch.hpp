// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

namespace rohit::serializer::detail {

// Check CPU instructions and OS register-state support before entering an AVX2 object file.
bool supports_avx2() noexcept;

} // namespace rohit::serializer::detail
