#include "schema.h"
#include <time.h>

/* Stop with a useful diagnostic on I/O, allocation, or codec failure. */
static void require(bool success, const char* message) {
  if (!success) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
  }
}
/* Allocate zeroed owning collection storage for this fixed fixture. */
static void* storage(size_t count, size_t width) {
  void* result = calloc(count, width);
  require(result != NULL, "Allocation failed");
  return result;
}
/* Assign an owned string, preserving the generated model's previous allocation. */
static void text(srl_string* result, const char* value) {
  require(srl_string_set_cstr(result, value) == srl_ok, "String allocation failed");
}
/* Construct all fields independently using generated defaults and typed C ownership. */
static void fixture(interop_message* value, int variant) {
  require(interop_message_init(value) == srl_ok, "Initialization failed");
  char label[128] = "Ada \"Lovelace\" \xf0\x9f\x9a\x80\n";
  const size_t prefix = strlen(label);
  memset(label + prefix, 'x', 64);
  label[prefix + 64] = 0;
  text(&value->text, label);
  value->numbers.data = storage(4, sizeof(*value->numbers.data));
  value->numbers.size = value->numbers.capacity = 4;
  const int32_t numbers[] = {INT32_MIN, -1, 0, INT32_MAX};
  memcpy(value->numbers.data, numbers, sizeof(numbers));
  value->decimals.data = storage(3, sizeof(*value->decimals.data));
  value->decimals.size = value->decimals.capacity = 3;
  const float decimals[] = {-0.0f, 1.5f, -2.25f};
  memcpy(value->decimals.data, decimals, sizeof(decimals));
  value->flags.data = storage(2, sizeof(*value->flags.data));
  value->flags.size = value->flags.capacity = 2;
  value->flags.data[1] = true;
  value->labels.data = storage(3, sizeof(*value->labels.data));
  value->labels.size = value->labels.capacity = 3;
  text(&value->labels.data[0], "");
  text(&value->labels.data[1], "\xc3\xa9");
  text(&value->labels.data[2], "\xf0\x9f\x9a\x80");
  value->children.data = storage(2, sizeof(*value->children.data));
  value->children.size = value->children.capacity = 2;
  for (size_t i = 0; i < 2; ++i) {
    require(interop_detail_init(&value->children.data[i]) == srl_ok, "Child initialization failed");
  }
  value->children.data[1].code = -9;
  text(&value->children.data[1].note, "child");
  value->states.data = storage(2, sizeof(*value->states.data));
  value->states.size = value->states.capacity = 2;
  value->states.data[0] = interop_state_paused;
  value->states.data[1] = interop_state_ready;
  value->counts.data = storage(3, sizeof(*value->counts.data));
  value->counts.size = value->counts.capacity = 3;
  text(&value->counts.data[0].key, "\xf0\x9f\x9a\x80");
  value->counts.data[0].value = UINT64_MAX;
  text(&value->counts.data[1].key, "\xc3\xa9");
  value->counts.data[1].value = 7;
  text(&value->counts.data[2].key, "a");
  value->indexed.data = storage(2, sizeof(*value->indexed.data));
  value->indexed.size = value->indexed.capacity = 2;
  for (size_t i = 0; i < 2; ++i) {
    require(interop_detail_init(&value->indexed.data[i].value) == srl_ok,
            "Map initialization failed");
  }
  value->indexed.data[0].key = UINT64_MAX;
  value->indexed.data[0].value.code = -9;
  text(&value->indexed.data[0].value.note, "child");
  value->toggles.data = storage(2, sizeof(*value->toggles.data));
  value->toggles.size = value->toggles.capacity = 2;
  value->toggles.data[0].key = true;
  text(&value->toggles.data[0].value, "yes");
  text(&value->toggles.data[1].value, "no");
  value->enums.data = storage(2, sizeof(*value->enums.data));
  value->enums.size = value->enums.capacity = 2;
  value->enums.data[0].key = interop_state_paused;
  value->enums.data[0].value = -2;
  value->enums.data[1].key = interop_state_ready;
  value->enums.data[1].value = 1;
  value->payload_index = variant;
  value->payload_number = -INT64_C(1234567890123456789);
  value->payload_ratio = -3.5f;
  value->payload_state = interop_state_paused;
}
/* Compare complete bytes without reading empty pointers. */
static bool equal(const srl_buffer* a, const srl_buffer* b) {
  return a->size == b->size && (!a->size || memcmp(a->data, b->data, a->size) == 0);
}
/* Form a bounded fixture path; directory creation belongs to the runner. */
static void path(char* result, size_t capacity, const char* directory, const char* suffix) {
  const int size = snprintf(result, capacity, "%s/%s", directory, suffix);
  require(size >= 0 && (size_t)size < capacity, "Fixture path too long");
}
/* Read an owned file with checked sizes; no generated model borrows this storage. */
static srl_buffer read_file(const char* name) {
  FILE* file = fopen(name, "rb");
  require(file != NULL, name);
  require(fseek(file, 0, SEEK_END) == 0, "Seek failed");
  const long length = ftell(file);
  require(length >= 0 && fseek(file, 0, SEEK_SET) == 0, "Length failed");
  srl_buffer result = {0};
  require(srl_reserve(&result, (size_t)length), "Allocation failed");
  result.size = (size_t)length;
  require(fread(result.data, 1, result.size, file) == result.size, "Read failed");
  require(fclose(file) == 0, "Close failed");
  return result;
}
/* Exchange every selected producer and assert canonical binary bytes and complete values. */
static void exchange(const char* directory, bool emit) {
  const char* protocols[] = {"JSON", "BINARY_NONE", "BINARY_INTEGER", "BINARY_STRING"};
  for (int variant = 0; variant < 3; ++variant) {
    interop_message expected = {0};
    fixture(&expected, variant);
    srl_buffer canonical = {0};
    require(interop_message_encode(&expected, srl_binary_none, &canonical) == srl_ok,
            "Encode failed");
    for (int p = 0; p < 4; ++p) {
      srl_buffer encoded = {0};
      require(interop_message_encode(&expected, (srl_protocol)p, &encoded) == srl_ok,
              "Encode failed");
      char name[4096], suffix[128];
      if (emit) {
        snprintf(suffix, sizeof(suffix), "c_%s_%d.bin", protocols[p], variant);
        path(name, sizeof(name), directory, suffix);
        FILE* file = fopen(name, "wb");
        require(file != NULL, name);
        require(fwrite(encoded.data, 1, encoded.size, file) == encoded.size, "Write failed");
        require(fclose(file) == 0, "Close failed");
      } else {
        path(name, sizeof(name), directory, "producers.txt");
        FILE* producers = fopen(name, "r");
        require(producers != NULL, name);
        char language[32];
        while (fscanf(producers, "%31s", language) == 1) {
          snprintf(suffix, sizeof(suffix), "%s_%s_%d.bin", language, protocols[p], variant);
          path(name, sizeof(name), directory, suffix);
          srl_buffer bytes = read_file(name), actual_bytes = {0};
          interop_message actual = {0};
          require(interop_message_decode(&actual, bytes.data, bytes.size, (srl_protocol)p, NULL) ==
                      srl_ok,
                  name);
          require(interop_message_encode(&actual, srl_binary_none, &actual_bytes) == srl_ok,
                  "Re-encode failed");
          require(equal(&canonical, &actual_bytes), "C value mismatch");
          require(p == srl_json || equal(&bytes, &encoded), "Canonical binary mismatch");
          interop_message_free(&actual);
          srl_buffer_free(&bytes);
          srl_buffer_free(&actual_bytes);
        }
        require(!ferror(producers) && fclose(producers) == 0, "Manifest read failed");
      }
      srl_buffer_free(&encoded);
    }
    srl_buffer_free(&canonical);
    interop_message_free(&expected);
  }
}
/* Measure complete owning calls, including allocation and replacement of prior results. */
static void benchmark(void) {
  const char* protocols[] = {"JSON", "BINARY_NONE", "BINARY_INTEGER", "BINARY_STRING"};
  const size_t iterations = 10000;
  interop_message value = {0};
  fixture(&value, 0);
  for (int protocol = 0; protocol < 4; ++protocol) {
    srl_buffer bytes = {0};
    interop_message copy = {0};
    for (size_t i = 0; i < 1000; ++i) {
      require(interop_message_encode(&value, (srl_protocol)protocol, &bytes) == srl_ok,
              "Encode failed");
      require(interop_message_decode(&copy, bytes.data, bytes.size, (srl_protocol)protocol, NULL) ==
                  srl_ok,
              "Decode failed");
    }
    clock_t start = clock();
    for (size_t i = 0; i < iterations; ++i) {
      require(interop_message_encode(&value, (srl_protocol)protocol, &bytes) == srl_ok,
              "Encode failed");
    }
    const double encode_seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    start = clock();
    for (size_t i = 0; i < iterations; ++i) {
      require(interop_message_decode(&copy, bytes.data, bytes.size, (srl_protocol)protocol, NULL) ==
                  srl_ok,
              "Decode failed");
    }
    const double decode_seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("c %s: %zu bytes; encode %.0f/s; decode %.0f/s\n", protocols[protocol], bytes.size,
           iterations / encode_seconds, iterations / decode_seconds);
    interop_message_free(&copy);
    srl_buffer_free(&bytes);
  }
  interop_message_free(&value);
}

/* Run the C producer/consumer while explicitly releasing all owning storage. */
int main(int argc, char** argv) {
  require(argc == 3 && (!strcmp(argv[2], "emit") || !strcmp(argv[2], "verify") ||
                        !strcmp(argv[2], "benchmark")),
          "Usage: main <directory> emit|verify|benchmark");
  if (!strcmp(argv[2], "benchmark")) {
    benchmark();
  } else {
    exchange(argv[1], !strcmp(argv[2], "emit"));
  }
  printf("c %s passed\n", argv[2]);
  return 0;
}
