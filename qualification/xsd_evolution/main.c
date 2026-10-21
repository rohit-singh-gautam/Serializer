// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t fixture_limit_bytes = 16u * 1024u * 1024u;

/* Stop before opening an output when any generated codec or typed-value assertion fails. */
static void require(bool condition, const char* message) {
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
  }
}

/* Read one bounded complete fixture into caller-owned storage. */
static srl_buffer read_file(const char* path) {
  FILE* file = fopen(path, "rb");
  require(file != NULL, "Cannot open input");
  require(fseek(file, 0, SEEK_END) == 0, "Cannot seek input");
  const long size = ftell(file);
  require(size >= 0 && (uintmax_t)size <= fixture_limit_bytes, "Input exceeds fixture bound");
  require(fseek(file, 0, SEEK_SET) == 0, "Cannot rewind input");
  srl_buffer result = {0};
  require(srl_reserve(&result, (size_t)size), "Cannot allocate input");
  result.size = (size_t)size;
  require(fread(result.data, 1, result.size, file) == result.size && fclose(file) == 0,
          "Cannot read input");
  return result;
}

/* Publish only an already encoded and validated complete artifact. */
static void write_file(const char* path, const srl_buffer* bytes) {
  FILE* file = fopen(path, "wb");
  require(file != NULL, "Cannot create output");
  require(fwrite(bytes->data, 1, bytes->size, file) == bytes->size && fclose(file) == 0,
          "Cannot finalize output");
}

/* Resolve a fixed artifact name below the orchestrator-created output directory. */
static void write_artifact(const char* directory, const char* name, const srl_buffer* bytes) {
  char path[4096];
  const int size = snprintf(path, sizeof(path), "%s/%s", directory, name);
  require(size >= 0 && (size_t)size < sizeof(path), "Output path exceeds fixture bound");
  write_file(path, bytes);
}

/* Check application fields and portable behavior independently of encoded byte comparisons. */
static void check_authored(const evolution_order* value) {
  require(value->id == 7 && srl_string_equal(&value->customer, "R&D"), "Authored scalars differ");
  require(value->note.has_value && srl_string_equal(&value->note.value, "memo"), "Authored note differs");
  require(evolution_order_has_id(value, 7) && !evolution_order_has_id(value, 8), "Authored behavior differs");
}

/* Encode through an actual native protocol, retaining the old public method signatures. */
static srl_buffer native_bytes(const evolution_order* value, srl_protocol protocol) {
  srl_buffer result = {0};
  require(evolution_order_encode(value, protocol, &result) == srl_ok, "Native encode failed");
  return result;
}

/* Qualify schema defaults, native identities and each version's XSD-derived XML association. */
int main(int argc, char** argv) {
  require(argc >= 4 && !strcmp(argv[1], "@variant@"), "Usage: VARIANT emit OUTDIR | check-xml/check-native INPUT OUTPUT");
  evolution_order value = {0};
  require(evolution_order_init(&value) == srl_ok, "Default construction failed");
  require(value.id == 7 && srl_string_equal(&value.customer, "native default") && !value.note.has_value,
          "Schema defaults differ");
  require(evolution_order_has_id(&value, 7) && !evolution_order_has_id(&value, 8), "Default behavior differs");
  if (!strcmp(argv[2], "emit")) {
    require(argc == 4, "emit requires an output directory");
    srl_buffer defaults = native_bytes(&value, srl_binary_none);
    require(srl_string_set_cstr(&value.customer, "R&D") == srl_ok, "Cannot set customer");
    value.note.has_value = true;
    require(srl_string_set_cstr(&value.note.value, "memo") == srl_ok, "Cannot set note");
    check_authored(&value);
    srl_buffer positional = native_bytes(&value, srl_binary_none);
    srl_buffer integer = native_bytes(&value, srl_binary_integer);
    srl_buffer string = native_bytes(&value, srl_binary_string);
    srl_buffer xml = {0};
    require(evolution_order_encode_xml(&value, &xml, NULL) == srl_ok, "XML encode failed");
    evolution_order projection = {0};
    require(evolution_order_decode_xml(&projection, xml.data, xml.size, NULL) == srl_ok,
            "XML projection decode failed");
    check_authored(&projection);
    srl_buffer projected = native_bytes(&projection, srl_binary_none);
    write_artifact(argv[3], "authored.xml", &xml);
    write_artifact(argv[3], "authored.bin", &positional);
    write_artifact(argv[3], "authored-integer.bin", &integer);
    write_artifact(argv[3], "authored-string.bin", &string);
    write_artifact(argv[3], "default.bin", &defaults);
    write_artifact(argv[3], "xml-projection.bin", &projected);
    srl_buffer_free(&xml); srl_buffer_free(&positional); srl_buffer_free(&integer);
    srl_buffer_free(&string); srl_buffer_free(&defaults); srl_buffer_free(&projected);
    evolution_order_free(&projection);
  } else {
    require(argc == 5, "check operation requires input and output");
    srl_buffer input = read_file(argv[3]), output = {0};
    if (!strcmp(argv[2], "check-xml")) {
      require(evolution_order_decode_xml(&value, input.data, input.size, NULL) == srl_ok, "XML decode failed");
      check_authored(&value);
      output = native_bytes(&value, srl_binary_none);
    } else {
      require(!strcmp(argv[2], "check-native"), "Unknown operation");
      require(evolution_order_decode(&value, input.data, input.size, srl_binary_none, NULL) == srl_ok,
              "Native decode failed");
      check_authored(&value);
      require(evolution_order_encode_xml(&value, &output, NULL) == srl_ok, "XML encode failed");
    }
    write_file(argv[4], &output);
    srl_buffer_free(&input); srl_buffer_free(&output);
  }
  evolution_order_free(&value);
  puts("C authored XSD evolution passed");
  return EXIT_SUCCESS;
}
