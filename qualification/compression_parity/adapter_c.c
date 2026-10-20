// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"
#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

/* Keep checks active under optimized native SDK builds. */
static void require(bool value, const char* message) {
  if (!value) {
    fprintf(stderr, "FAILED: %s\n", message);
    exit(1);
  }
}
/* Read small qualification files without C-string interpretation of their payload. */
static srl_buffer load(const char* path) {
  FILE* input = fopen(path, "rb");
  require(input != NULL, path);
  require(fseek(input, 0, SEEK_END) == 0, "seek file");
  const long size = ftell(input);
  require(size >= 0 && size <= 8388608, "bounded file size");
  require(fseek(input, 0, SEEK_SET) == 0, "rewind file");
  srl_buffer result = {0};
  require(srl_reserve(&result, (size_t)size), "file allocation");
  result.size = (size_t)size;
  require(fread(result.data, 1, result.size, input) == result.size, "read file");
  require(fclose(input) == 0, "close input");
  return result;
}
/* Publish only complete successful producer bytes. */
static void save(const char* path, const srl_buffer* value) {
  FILE* output = fopen(path, "wb");
  require(output != NULL, path);
  require(fwrite(value->data, 1, value->size, output) == value->size, "write file");
  require(fclose(output) == 0, "close output");
}
/* Normalize the explicit standard format rather than sniffing input. */
static serializer_compression_format format(const char* name) {
  const char* names[] = {"NONE", "GZIP", "ZLIB", "DEFLATE", "ZSTD", "LZ4"};
  const serializer_compression_format values[] = {
      serializer_compression_none,    serializer_compression_gzip, serializer_compression_zlib,
      serializer_compression_deflate, serializer_compression_zstd, serializer_compression_lz4};
  for (size_t index = 0; index < sizeof(names) / sizeof(*names); ++index) {
    if (!strcmp(name, names[index])) {
      return values[index];
    }
  }
  require(false, "Unknown format");
  return serializer_compression_none;
}
/* Compare owned byte extents, including empty identity payloads. */
static bool equal(const srl_buffer* first, const srl_buffer* second) {
  return first->size == second->size &&
         (!first->size || !memcmp(first->data, second->data, first->size));
}
/* Replay every mandatory case and complete generated native model without optional skips. */
int main(int argc, char** argv) {
  require(argc == 3, "Expected mode and corpus directory");
#if defined(_WIN32)
  require(_chdir(argv[2]) == 0, "corpus directory");
#else
  require(chdir(argv[2]) == 0, "corpus directory");
#endif
  const char* kinds[] = {"NONE", "GZIP", "ZLIB", "DEFLATE", "ZSTD", "LZ4"};
  const char* protocols[] = {"JSON", "BINARY_NONE", "BINARY_INTEGER", "BINARY_STRING"};
  for (size_t index = 0; index < sizeof(kinds) / sizeof(*kinds); ++index) {
    require(serializer_compression_available(format(kinds[index])), "Missing C provider");
  }
  if (!strcmp(argv[1], "produce")) {
    account_record value = {0};
    require(account_record_init(&value) == srl_ok, "init value");
    value.account_id = 42;
    require(srl_string_set_cstr(&value.label, "shared \xce\xbb \xf0\x9f\x99\x82") == srl_ok,
            "Unicode value");
    for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
      srl_buffer plain = {0};
      require(account_record_encode(&value, (srl_protocol)protocol, &plain) == srl_ok,
              "native encode");
      for (size_t index = 0; index < sizeof(kinds) / sizeof(*kinds); ++index) {
        serializer_compression_encode_options options =
            serializer_compression_default_encode_options();
        options.format = format(kinds[index]);
        options.level = options.format == serializer_compression_none ||
                                options.format == serializer_compression_lz4
                            ? 0
                            : 3;
        srl_buffer frame = {0};
        srl_status native = srl_invalid;
        require(serializer_compression_account_record_encode(&value, (srl_protocol)protocol,
                                                             options, &frame, &native) ==
                        serializer_compression_ok &&
                    native == srl_ok,
                "typed encode");
        serializer_compression_encode_options exact = options;
        exact.max_input_bytes = plain.size;
        exact.max_output_bytes = frame.size;
        srl_buffer repeated = {0};
        require(serializer_compression_account_record_encode(&value, (srl_protocol)protocol, exact,
                                                             &repeated, &native) ==
                        serializer_compression_ok &&
                    equal(&frame, &repeated),
                "Exact encode admission");
        --exact.max_input_bytes;
        require(serializer_compression_account_record_encode(&value, (srl_protocol)protocol, exact,
                                                             &repeated, &native) ==
                    serializer_compression_resource_limit,
                "One-tight encode input");
        ++exact.max_input_bytes;
        --exact.max_output_bytes;
        require(serializer_compression_account_record_encode(&value, (srl_protocol)protocol, exact,
                                                             &repeated, &native) ==
                    serializer_compression_resource_limit,
                "One-tight encode output");
        srl_buffer_free(&repeated);
        char path[128];
        (void)snprintf(path, sizeof(path), "c-%s-%s.frame", protocols[protocol], kinds[index]);
        save(path, &frame);
        (void)snprintf(path, sizeof(path), "c-%s-%s.plain", protocols[protocol], kinds[index]);
        save(path, &plain);
        srl_buffer_free(&frame);
      }
      srl_buffer_free(&plain);
    }
    account_record_free(&value);
    puts("{\"produced\":24,\"encode_bounds\":72}");
    return 0;
  }
  size_t decisions = 0, exchanges = 0;
  char line[4096];
  FILE* cases = fopen("cases.tsv", "r");
  require(cases != NULL, "cases manifest");
  while (fgets(line, sizeof(line), cases)) {
    char* id = strtok(line, "\t");
    char* kind = strtok(NULL, "\t");
    char* source = strtok(NULL, "\t");
    char* oracle = strtok(NULL, "\t");
    char* wanted = strtok(NULL, "\t");
    char* input = strtok(NULL, "\t");
    char* output = strtok(NULL, "\t");
    char* window = strtok(NULL, "\t\r\n");
    require(window != NULL, "case fields");
    serializer_compression_decode_options policy = serializer_compression_default_decode_options();
    policy.format = format(kind);
    policy.max_input_bytes = (size_t)strtoull(input, NULL, 10);
    policy.max_output_bytes = (size_t)strtoull(output, NULL, 10);
    policy.max_window_bytes = (size_t)strtoull(window, NULL, 10);
    srl_buffer packed = load(source), plain = {0};
    const serializer_compression_code code =
        serializer_compression_decompress(packed.data, packed.size, policy, &plain);
    const char* observed = code == serializer_compression_ok               ? "ok"
                           : code == serializer_compression_invalid_data   ? "invalid_data"
                           : code == serializer_compression_resource_limit ? "resource_limit"
                                                                           : "unexpected";
    if (strcmp(observed, wanted)) {
      fprintf(stderr, "%s: %s != %s\n", id, observed, wanted);
    }
    require(!strcmp(observed, wanted), id);
    if (code == serializer_compression_ok) {
      srl_buffer expected = load(oracle);
      require(equal(&plain, &expected), id);
      srl_buffer_free(&expected);
    }
    srl_buffer_free(&packed);
    srl_buffer_free(&plain);
    ++decisions;
  }
  require(fclose(cases) == 0, "close cases");
  FILE* jobs = fopen("exchange.tsv", "r");
  require(jobs != NULL, "exchange manifest");
  while (fgets(line, sizeof(line), jobs)) {
    char* id = strtok(line, "\t");
    char* protocol = strtok(NULL, "\t");
    char* kind = strtok(NULL, "\t");
    char* source = strtok(NULL, "\t");
    char* oracle = strtok(NULL, "\t\r\n");
    require(oracle != NULL, "exchange fields");
    int selected = -1;
    for (int index = srl_json; index <= srl_binary_string; ++index) {
      if (!strcmp(protocol, protocols[index])) {
        selected = index;
      }
    }
    require(selected >= 0, "native protocol");
    serializer_compression_decode_options policy = serializer_compression_default_decode_options();
    policy.format = format(kind);
    policy.max_window_bytes = 8388608;
    srl_buffer frame = load(source), plain = {0}, expected = load(oracle);
    account_record value = {0};
    require(account_record_init(&value) == srl_ok, "init candidate");
    srl_status native = srl_invalid;
    require(serializer_compression_account_record_decode(&value, frame.data, frame.size,
                                                         (srl_protocol)selected, NULL, policy,
                                                         &native) == serializer_compression_ok &&
                native == srl_ok,
            id);
    require(value.account_id == 42 &&
                srl_string_equal(&value.label, "shared \xce\xbb \xf0\x9f\x99\x82"),
            id);
    require(serializer_compression_decompress(frame.data, frame.size, policy, &plain) ==
                    serializer_compression_ok &&
                equal(&plain, &expected),
            id);
    account_record_free(&value);
    srl_buffer_free(&frame);
    srl_buffer_free(&plain);
    srl_buffer_free(&expected);
    ++exchanges;
  }
  require(fclose(jobs) == 0, "close exchanges");
  printf("{\"decisions\":%zu,\"exchanges\":%zu}\n", decisions, exchanges);
  return 0;
}
