// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "benchmark_sink.h"

/* Keep the caller's complete result observable without reading it or allocating storage.
 * Compile this translation unit without LTO; the dormant address is never dereferenced. */
void serializer_benchmark_observe(const void* value) {
  static const void* volatile observed;
  observed = value;
}
