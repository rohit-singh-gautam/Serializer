// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"
#include "benchmark_sink.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct {
  srl_buffer bytes;
  xml_example_order model;
  bool owning;
} benchmark_result;

/* Reject incomplete I/O, provider failures and changed measured values in optimized builds. */
static void require(bool condition, const char* message) {
  if (!condition) { fprintf(stderr, "%s\n", message); exit(EXIT_FAILURE); }
}

/* Own one complete input outside the measured codec calls. */
static srl_buffer read_input(const char* path) {
  FILE* file = fopen(path, "rb");
  require(file != NULL, "Cannot open benchmark input");
  require(fseek(file, 0, SEEK_END) == 0, "Cannot seek benchmark input");
  const long length = ftell(file);
  require(length >= 0 && length <= 16 * 1024 * 1024, "Benchmark input exceeds selected bound");
  require(fseek(file, 0, SEEK_SET) == 0, "Cannot rewind benchmark input");
  srl_buffer result = {0};
  require(srl_reserve(&result, (size_t)length), "Cannot allocate benchmark input");
  result.size = (size_t)length;
  require(fread(result.data, 1, result.size, file) == result.size, "Cannot read benchmark input");
  require(fclose(file) == 0, "Cannot close benchmark input");
  return result;
}

/* Publish only the separately checked canonical wire or final XML after timing completes. */
static void write_output(const char* path, const srl_buffer* bytes) {
  FILE* file = fopen(path, "wb");
  require(file != NULL, "Cannot open benchmark output");
  require(fwrite(bytes->data, 1, bytes->size, file) == bytes->size, "Cannot write benchmark output");
  require(fclose(file) == 0, "Cannot close benchmark output");
}

/* Release the final retained owning result after all timing and validation complete. */
static void clear_result(benchmark_result* result) {
  srl_buffer_free(&result->bytes);
  xml_example_order_free(&result->model);
  memset(result, 0, sizeof(*result));
}

/* Execute a public complete-message codec and retain its result across iterations. */
static void operation(const char* mode, const xml_example_order* value,
    const srl_buffer* input, const srl_buffer* canonical, benchmark_result* result) {
  (void)input;
  /* Public destination APIs replace their previous owning result transactionally. */
  if (strcmp(mode, "native-encode") == 0) {
    require(xml_example_order_encode(value, srl_binary_none, &result->bytes) == srl_ok, "Native encode failed");
  } else if (strcmp(mode, "native-decode") == 0) {
    require(xml_example_order_decode(&result->model, canonical->data, canonical->size, srl_binary_none, NULL) == srl_ok, "Native decode failed");
    result->owning = true;
  }
  // BENCHMARK_XML_OPERATIONS
  else { require(false, "Unknown benchmark operation"); }
  serializer_benchmark_observe(result);
}

/* Measure three warm batches and five complete owning batches with validation outside timing. */
int main(int argc, char** argv) {
  require(argc == 5, "Usage: benchmark INPUT MODE OUTPUT REPETITIONS");
  const unsigned long repetitions = strtoul(argv[4], NULL, 10);
  require(repetitions > 0 && repetitions <= INT_MAX, "Invalid benchmark repetitions");
  const char* mode = argv[2];
  srl_buffer input = read_input(argv[1]);
  xml_example_order value = {0};
  // BENCHMARK_SETUP
  srl_buffer canonical = {0};
  require(xml_example_order_encode(&value, srl_binary_none, &canonical) == srl_ok, "Canonical encode failed");
  benchmark_result retained = {0};
  for (unsigned int batch = 0; batch < 3; ++batch) {
    for (unsigned long iteration = 0; iteration < repetitions; ++iteration) {
      operation(mode, &value, &input, &canonical, &retained);
    }
  }
  LARGE_INTEGER frequency;
  require(QueryPerformanceFrequency(&frequency) != 0, "Missing monotonic clock");
  unsigned long long samples[5] = {0};
  for (size_t batch = 0; batch < sizeof(samples) / sizeof(samples[0]); ++batch) {
    LARGE_INTEGER start, finish;
    require(QueryPerformanceCounter(&start) != 0, "Cannot sample monotonic clock");
    for (unsigned long iteration = 0; iteration < repetitions; ++iteration) {
      operation(mode, &value, &input, &canonical, &retained);
    }
    require(QueryPerformanceCounter(&finish) != 0, "Cannot sample monotonic clock");
    samples[batch] = (unsigned long long)((double)(finish.QuadPart - start.QuadPart) * 1e9 / (double)frequency.QuadPart);
  }
  xml_example_order checked = {0};
  const xml_example_order* result = retained.owning ? &retained.model : &checked;
  if (!retained.owning) {
    // BENCHMARK_RESULT
  }
  srl_buffer verification = {0};
  require(xml_example_order_encode(result, srl_binary_none, &verification) == srl_ok, "Result encode failed");
  require(verification.size == canonical.size && memcmp(verification.data, canonical.data, canonical.size) == 0, "Measured result changed native values");
  write_output(argv[3], &canonical);
  // BENCHMARK_XML_OUTPUT
  printf("{\"wall_ns\":[%llu,%llu,%llu,%llu,%llu],\"repetitions\":%lu,"
      "\"allocation_bytes\":null,\"allocation_method\":\"Not instrumented; independent process high-water includes native/provider storage\"}\n",
      samples[0], samples[1], samples[2], samples[3], samples[4], repetitions);
  srl_buffer_free(&verification);
  xml_example_order_free(&checked);
  clear_result(&retained);
  xml_example_order_free(&value);
  srl_buffer_free(&input);
  srl_buffer_free(&canonical);
  return 0;
}
