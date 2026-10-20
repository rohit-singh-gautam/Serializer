// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

/* Borrow one owning result during an opaque optimization barrier; no pointee is retained. */
void serializer_benchmark_observe(const void* value);
