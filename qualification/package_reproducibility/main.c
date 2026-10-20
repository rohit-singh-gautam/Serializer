// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t fixture_limit_bytes = 16u * 1024u * 1024u;
static const unsigned permutation_count = 6;

typedef struct fixture_rows {
  char* storage;
  char** lines;
  size_t size;
} fixture_rows;

/* Stop a failed producer before publishing any archive or success fingerprint. */
static void require(bool condition, const char* message) {
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
  }
}

/* Copy one fixture-owned metadata field into independent C catalog ownership. */
static char* copy_text(const char* text) {
  const size_t size = strlen(text);
  char* result = (char*)malloc(size + 1);
  require(result != NULL, "Cannot allocate fixture metadata");
  memcpy(result, text, size + 1);
  return result;
}

/* Read the bounded UTF-8/LF fixture without normalizing target strings or Unicode names. */
static fixture_rows read_rows(const char* directory, const char* kind, const char* name) {
  char path[4096];
  const int count = snprintf(path, sizeof(path), "%s/%s/%s", directory, kind, name);
  require(count >= 0 && (size_t)count < sizeof(path), "Fixture path exceeds bound");
  FILE* file = fopen(path, "rb");
  require(file != NULL, "Cannot open fixture TSV");
  require(!fseek(file, 0, SEEK_END), "Cannot seek fixture TSV");
  const long extent = ftell(file);
  require(extent >= 0 && (uintmax_t)extent <= fixture_limit_bytes, "Fixture TSV exceeds bound");
  require(!fseek(file, 0, SEEK_SET), "Cannot rewind fixture TSV");
  fixture_rows result = {0};
  result.storage = (char*)malloc((size_t)extent + 1);
  require(result.storage != NULL, "Cannot allocate fixture TSV");
  require(fread(result.storage, 1, (size_t)extent, file) == (size_t)extent && !fclose(file),
          "Cannot read fixture TSV");
  result.storage[extent] = '\0';
  require(strlen(result.storage) == (size_t)extent && !strchr(result.storage, '\r'), "TSV must use UTF-8/LF");
  for (const char* cursor = result.storage; *cursor; ++cursor) {
    if (*cursor == '\n') {
      ++result.size;
    }
  }
  if (extent && result.storage[extent - 1] != '\n') {
    ++result.size;
  }
  require(result.size <= SIZE_MAX / sizeof(*result.lines), "Too many fixture rows");
  result.lines = (char**)calloc(result.size ? result.size : 1, sizeof(*result.lines));
  require(result.lines != NULL, "Cannot allocate fixture rows");
  char* cursor = result.storage;
  for (size_t index = 0; index < result.size; ++index) {
    result.lines[index] = cursor;
    char* end = strchr(cursor, '\n');
    if (end) {
      *end = '\0';
      cursor = end + 1;
    }
    require(*result.lines[index] != '\0', "Empty fixture row");
  }
  return result;
}

/* Release the temporary parser inventory after all catalog metadata has been copied. */
static void free_rows(fixture_rows* rows) {
  free(rows->lines);
  free(rows->storage);
  memset(rows, 0, sizeof(*rows));
}

/* Split an exact headerless TSV row while retaining an empty final payload column. */
static void fields(char* line, char** result, size_t size) {
  for (size_t index = 0; index < size; ++index) {
    result[index] = line;
    char* tab = strchr(line, '\t');
    require((tab != NULL) == (index + 1 < size), "Incorrect fixture field count");
    if (tab) {
      *tab = '\0';
      line = tab + 1;
    }
  }
}

/* Decode only the explicitly specified ASCII hexadecimal payload alphabet. */
static unsigned hex_digit(unsigned char value) {
  if (value >= '0' && value <= '9') { return (unsigned)(value - '0'); }
  if (value >= 'a' && value <= 'f') { return (unsigned)(value - 'a') + 10u; }
  if (value >= 'A' && value <= 'F') { return (unsigned)(value - 'A') + 10u; }
  require(false, "Invalid hexadecimal fixture payload");
  return 0;
}

/* Preserve empty, binary and UTF-8 payloads as exact bytes rather than native strings. */
static srl_buffer decode_hex(const char* value) {
  const size_t size = strlen(value);
  require(size % 2u == 0, "Odd hexadecimal fixture payload");
  srl_buffer result = {0};
  result.size = size / 2u;
  result.capacity = result.size;
  if (result.size) {
    result.data = (uint8_t*)malloc(result.size);
    require(result.data != NULL, "Cannot allocate fixture payload");
  }
  for (size_t index = 0; index < result.size; ++index) {
    result.data[index] = (uint8_t)((hex_digit((unsigned char)value[index * 2u]) << 4) |
                                 hex_digit((unsigned char)value[index * 2u + 1]));
  }
  return result;
}

/* Apply one common input permutation; the generated writer alone chooses output ordering. */
static size_t* permutation(size_t size, unsigned selected) {
  require(size <= SIZE_MAX / sizeof(size_t), "Permutation extent overflow");
  size_t* result = (size_t*)malloc((size ? size : 1) * sizeof(*result));
  require(result != NULL && selected < permutation_count, "Invalid fixture permutation");
  size_t written = 0;
  if (selected < 4u) {
    for (size_t index = 0; index < size; ++index) {
      result[written++] = selected == 1u ? size - index - 1 :
          selected == 2u ? (index + 1) % size : selected == 3u ? (index + size / 2u) % size : index;
    }
  } else {
    for (unsigned group = 0; group < 2u; ++group) {
      for (size_t index = 0; index < size; ++index) {
        const size_t value = selected == 4u ? index : size - index - 1;
        if (value % 2u == (selected == 4u ? group : 1u - group)) {
          result[written++] = value;
        }
      }
    }
  }
  require(written == size, "Incomplete fixture permutation");
  return result;
}

/* Parse all payload fields before reordering the borrowed input list. */
static @p@_binary_part* read_parts(const char* directory, const char* kind, unsigned selected,
                                unsigned method, size_t* count) {
  fixture_rows rows = read_rows(directory, kind, "parts.tsv");
  @p@_binary_part* result = (@p@_binary_part*)calloc(rows.size ? rows.size : 1, sizeof(*result));
  require(result != NULL, "Cannot allocate fixture part inventory");
  size_t* order = permutation(rows.size, selected);
  for (size_t index = 0; index < rows.size; ++index) {
    char* values[4];
    fields(rows.lines[order[index]], values, sizeof(values) / sizeof(values[0]));
    require(!strcmp(values[2], "stored") || !strcmp(values[2], "deflate"), "Unknown TSV storage");
    result[index].name = copy_text(values[0]);
    result[index].content_type = copy_text(values[1]);
    result[index].bytes = decode_hex(values[3]);
    result[index].method = method;
  }
  *count = rows.size;
  free(order);
  free_rows(&rows);
  return result;
}

/* Parse fixed source-local IDs and retain authored target selectors exactly. */
static @p@_relationship* read_links(const char* directory, unsigned selected, size_t* count) {
  fixture_rows rows = read_rows(directory, "opc", "links.tsv");
  @p@_relationship* result = (@p@_relationship*)calloc(rows.size ? rows.size : 1, sizeof(*result));
  require(result != NULL, "Cannot allocate fixture relationship inventory");
  size_t* order = permutation(rows.size, selected);
  for (size_t index = 0; index < rows.size; ++index) {
    char* values[5];
    fields(rows.lines[order[index]], values, sizeof(values) / sizeof(values[0]));
    result[index].source = copy_text(values[0]);
    result[index].id = copy_text(values[1]);
    result[index].type = copy_text(values[2]);
    result[index].target = copy_text(values[3]);
    require(!strcmp(values[4], "internal") || !strcmp(values[4], "external"), "Unknown target mode");
    result[index].external = !strcmp(values[4], "external");
  }
  *count = rows.size;
  free(order);
  free_rows(&rows);
  return result;
}

/* Report the actual linked native providers and unchanged writer compression defaults. */
static void print_provider(void) {
  printf("{\"implementation\":\"zlib\",\"version\":\"%s\",\"options\":{"
         "\"level\":6,\"window_bits\":-15,\"mem_level\":8,\"strategy\":\"Z_DEFAULT_STRATEGY\","
         "\"wrapper\":\"raw\",\"xml_provider\":\"libxml2\",\"xml_version\":\"%s\"},"
         "\"metadata_method\":0}", zlibVersion(), xmlParserVersion);
}

/* Produce one real archive through the existing generated public helpers in a fresh process. */
int main(int argc, char** argv) {
  if (argc == 2 && !strcmp(argv[1], "--provider")) {
    print_provider();
    putchar('\n');
    return 0;
  }
  require(argc == 5 || argc == 6, "Expected fixture-dir profile permutation output [force64 0|1]");
  const bool opc = !strcmp(argv[2], "opc-stored") || !strcmp(argv[2], "opc-deflate");
  const bool deflate = !strcmp(argv[2], "zip-deflate") || !strcmp(argv[2], "opc-deflate");
  require(opc || !strcmp(argv[2], "zip-stored") || !strcmp(argv[2], "zip-deflate"), "Unknown profile");
  errno = 0;
  char* end = NULL;
  const unsigned long selected = strtoul(argv[3], &end, 10);
  require(!errno && end && !*end && end != argv[3] && selected < permutation_count, "Invalid permutation");
  const bool force = argc == 6 && !strcmp(argv[5], "1");
  require(argc == 5 || !strcmp(argv[5], "0") || force, "Invalid forced ZIP64 selector");
  size_t count = 0, links = 0;
  @p@_binary_part* parts = read_parts(argv[1], opc ? "opc" : "zip", (unsigned)selected, deflate ? 8u : 0u, &count);
  @p@_budget budget = @p@_default_budget();
  budget.force_zip64 = force;
  srl_buffer output = {0};
  int status;
  if (opc) {
    @p@_catalog catalog = {0};
    catalog.parts = (@p@_catalog_part*)calloc(count ? count : 1, sizeof(*catalog.parts));
    require(catalog.parts != NULL, "Cannot allocate owned OPC catalog");
    catalog.parts_size = count;
    for (size_t index = 0; index < count; ++index) { catalog.parts[index].value = parts[index]; }
    free(parts);
    catalog.links = read_links(argv[1], (unsigned)selected, &links);
    catalog.links_size = links;
    status = @p@_opc_write(&catalog, &output, &budget);
    @p@_catalog_free(&catalog);
  } else {
    status = @p@_zip_write(parts, count, &output, &budget);
    for (size_t index = 0; index < count; ++index) {
      free(parts[index].name);
      free(parts[index].content_type);
      srl_buffer_free(&parts[index].bytes);
    }
    free(parts);
  }
  if (status) {
    fprintf(stderr, "Package status=%d part=%s field=%s link=%s\n", status, budget.part, budget.field, budget.link);
  }
  require(!status, "Generated package producer failed");
  FILE* file = fopen(argv[4], "wb");
  require(file != NULL, "Cannot create producer output");
  require(fwrite(output.data, 1, output.size, file) == output.size && !fclose(file), "Cannot publish producer output");
  printf("{\"profile\":\"%s\",\"permutation\":%lu,\"parts\":%zu,\"relationships\":%zu,"
         "\"force_zip64\":%s,\"provider\":", argv[2], selected, count, links, force ? "true" : "false");
  print_provider();
  puts("}");
  srl_buffer_free(&output);
  return 0;
}
