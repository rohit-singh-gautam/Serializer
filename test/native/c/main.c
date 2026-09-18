#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static size_t fail_after = SIZE_MAX;
static size_t live_allocations = 0;
/* Inject allocation failure without changing standard allocator ownership. */
static void* test_malloc(size_t size) {
  if (fail_after == 0) {
    return NULL;
  }
  if (fail_after != SIZE_MAX) {
    --fail_after;
  }
  void* result = malloc(size);
  if (result) {
    ++live_allocations;
  }
  return result;
}
/* Failed reallocation retains the original block, matching the C contract. */
static void* test_realloc(void* block, size_t size) {
  if (fail_after == 0) {
    return NULL;
  }
  if (fail_after != SIZE_MAX) {
    --fail_after;
  }
  const int fresh = block == NULL;
  void* result = realloc(block, size);
  if (result && fresh) {
    ++live_allocations;
  }
  return result;
}
/* Track every owned allocation released by generated cleanup. */
static void test_free(void* block) {
  if (block) {
    --live_allocations;
  }
  free(block);
}
#define malloc test_malloc
#define realloc test_realloc
#define free test_free
#include "schema.h"
/* Stop with a useful diagnostic on I/O, allocation, or codec failure. */
static void require(bool success, const char* message) {
  if (!success) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
  }
}
/* Check target-compiled defaults independently, including each zero's sign bit. */
static void check_floating_defaults(const check_floating_default_value* value) {
  require(value->positive_float == 2147483648.0f, "Positive float default changed");
  require(value->negative_float == -4294967296.0f, "Negative float default changed");
  require(value->positive_double == 9223372036854775808.0, "Positive double default changed");
  require(value->negative_double == -9223372036854775808.0, "Negative double default changed");
  require(value->scientific_float == 1250.0f, "Scientific float default changed");
  require(value->scientific_double == -0.025, "Scientific double default changed");
  require(value->negative_zero_float == 0 && signbit(value->negative_zero_float),
          "Negative float zero default changed");
  require(value->negative_zero_double == 0 && signbit(value->negative_zero_double),
          "Negative double zero default changed");
  require(value->zero_float == 0 && !signbit(value->zero_float), "Float zero default changed");
  require(value->zero_double == 0 && !signbit(value->zero_double), "Double zero default changed");
}
/* Exercise initialization, absent keyed fields, and complete protocol round trips. */
static void floating_defaults(void) {
  check_floating_default_value value = {0}, copy = {0};
  require(check_floating_default_value_init(&value) == srl_ok, "Floating default init failed");
  check_floating_defaults(&value);
  const srl_protocol protocols[] = {srl_json, srl_binary_none, srl_binary_integer,
                                    srl_binary_string};
  for (size_t i = 0; i < sizeof(protocols) / sizeof(protocols[0]); ++i) {
    srl_buffer encoded = {0};
    require(check_floating_default_value_encode(&value, protocols[i], &encoded) == srl_ok,
            "Floating default encode failed");
    require(check_floating_default_value_decode(&copy, encoded.data, encoded.size, protocols[i],
                                                NULL) == srl_ok,
            "Floating default decode failed");
    check_floating_defaults(&copy);
    srl_buffer_free(&encoded);
    if (protocols[i] != srl_binary_none) {
      const bool json = protocols[i] == srl_json;
      const char* empty = json ? "{}" : "\0";
      require(check_floating_default_value_decode(&copy, (const uint8_t*)empty,
                                                  json ? sizeof("{}") - 1 : sizeof("\0") - 1,
                                                  protocols[i], NULL) == srl_ok,
              "Absent fields did not retain floating defaults");
      check_floating_defaults(&copy);
    }
  }
  check_floating_default_value_free(&copy);
  check_floating_default_value_free(&value);
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

/* Verify failure is transactional even when replacing nonempty initialized models. */
static srl_status decode(int kind, const srl_buffer* input, srl_protocol protocol,
                         const srl_limits* limits, srl_buffer* result) {
  switch (kind) {
  case 0: {
    interop_message value = {0};
    require(interop_message_init(&value) == srl_ok, "Init failed");
    srl_buffer before = {0}, after = {0};
    require(interop_message_encode(&value, srl_json, &before) == srl_ok, "Encode failed");
    srl_status status = interop_message_decode(&value, input->data, input->size, protocol, limits);
    if (status == srl_ok) {
      status = interop_message_encode(&value, srl_json, result);
    } else {
      require(interop_message_encode(&value, srl_json, &after) == srl_ok, "Encode failed");
      require(before.size == after.size && !memcmp(before.data, after.data, before.size),
              "Failure changed destination");
    }
    interop_message_free(&value);
    srl_buffer_free(&before);
    srl_buffer_free(&after);
    return status;
  }
  case 1: {
    check_text_value value = {0};
    require(check_text_value_init(&value) == srl_ok, "Init failed");
    srl_buffer before = {0}, after = {0};
    require(check_text_value_encode(&value, srl_json, &before) == srl_ok, "Encode failed");
    srl_status status = check_text_value_decode(&value, input->data, input->size, protocol, limits);
    if (status == srl_ok) {
      status = check_text_value_encode(&value, srl_json, result);
    } else {
      require(check_text_value_encode(&value, srl_json, &after) == srl_ok, "Encode failed");
      require(before.size == after.size && !memcmp(before.data, after.data, before.size),
              "Failure changed destination");
    }
    check_text_value_free(&value);
    srl_buffer_free(&before);
    srl_buffer_free(&after);
    return status;
  }
  case 2: {
    check_integer_value value = {0};
    require(check_integer_value_init(&value) == srl_ok, "Init failed");
    srl_buffer before = {0}, after = {0};
    require(check_integer_value_encode(&value, srl_json, &before) == srl_ok, "Encode failed");
    srl_status status =
        check_integer_value_decode(&value, input->data, input->size, protocol, limits);
    if (status == srl_ok) {
      status = check_integer_value_encode(&value, srl_json, result);
    } else {
      require(check_integer_value_encode(&value, srl_json, &after) == srl_ok, "Encode failed");
      require(before.size == after.size && !memcmp(before.data, after.data, before.size),
              "Failure changed destination");
    }
    check_integer_value_free(&value);
    srl_buffer_free(&before);
    srl_buffer_free(&after);
    return status;
  }
  case 3: {
    check_bool_value value = {0};
    require(check_bool_value_init(&value) == srl_ok, "Init failed");
    srl_buffer before = {0}, after = {0};
    require(check_bool_value_encode(&value, srl_json, &before) == srl_ok, "Encode failed");
    srl_status status = check_bool_value_decode(&value, input->data, input->size, protocol, limits);
    if (status == srl_ok) {
      status = check_bool_value_encode(&value, srl_json, result);
    } else {
      require(check_bool_value_encode(&value, srl_json, &after) == srl_ok, "Encode failed");
      require(before.size == after.size && !memcmp(before.data, after.data, before.size),
              "Failure changed destination");
    }
    check_bool_value_free(&value);
    srl_buffer_free(&before);
    srl_buffer_free(&after);
    return status;
  }
  case 4: {
    check_float_value value = {0};
    require(check_float_value_init(&value) == srl_ok, "Init failed");
    srl_buffer before = {0}, after = {0};
    require(check_float_value_encode(&value, srl_json, &before) == srl_ok, "Encode failed");
    srl_status status =
        check_float_value_decode(&value, input->data, input->size, protocol, limits);
    if (status == srl_ok) {
      status = check_float_value_encode(&value, srl_json, result);
    } else {
      require(check_float_value_encode(&value, srl_json, &after) == srl_ok, "Encode failed");
      require(before.size == after.size && !memcmp(before.data, after.data, before.size),
              "Failure changed destination");
    }
    check_float_value_free(&value);
    srl_buffer_free(&before);
    srl_buffer_free(&after);
    return status;
  }
  case 5: {
    check_bytes_value value = {0};
    require(check_bytes_value_init(&value) == srl_ok, "Init failed");
    srl_buffer before = {0}, after = {0};
    require(check_bytes_value_encode(&value, srl_json, &before) == srl_ok, "Encode failed");
    srl_status status =
        check_bytes_value_decode(&value, input->data, input->size, protocol, limits);
    if (status == srl_ok) {
      status = check_bytes_value_encode(&value, srl_json, result);
    } else {
      require(check_bytes_value_encode(&value, srl_json, &after) == srl_ok, "Encode failed");
      require(before.size == after.size && !memcmp(before.data, after.data, before.size),
              "Failure changed destination");
    }
    check_bytes_value_free(&value);
    srl_buffer_free(&before);
    srl_buffer_free(&after);
    return status;
  }
  case 6: {
    check_empty value = {0};
    require(check_empty_init(&value) == srl_ok, "Init failed");
    srl_buffer before = {0}, after = {0};
    require(check_empty_encode(&value, srl_json, &before) == srl_ok, "Encode failed");
    srl_status status = check_empty_decode(&value, input->data, input->size, protocol, limits);
    if (status == srl_ok) {
      status = check_empty_encode(&value, srl_json, result);
    } else {
      require(check_empty_encode(&value, srl_json, &after) == srl_ok, "Encode failed");
      require(before.size == after.size && !memcmp(before.data, after.data, before.size),
              "Failure changed destination");
    }
    check_empty_free(&value);
    srl_buffer_free(&before);
    srl_buffer_free(&after);
    return status;
  }
  case 7: {
    check_recursive value = {0};
    require(check_recursive_init(&value) == srl_ok, "Init failed");
    srl_buffer before = {0}, after = {0};
    require(check_recursive_encode(&value, srl_json, &before) == srl_ok, "Encode failed");
    srl_status status = check_recursive_decode(&value, input->data, input->size, protocol, limits);
    if (status == srl_ok) {
      status = check_recursive_encode(&value, srl_json, result);
    } else {
      require(check_recursive_encode(&value, srl_json, &after) == srl_ok, "Encode failed");
      require(before.size == after.size && !memcmp(before.data, after.data, before.size),
              "Failure changed destination");
    }
    check_recursive_free(&value);
    srl_buffer_free(&before);
    srl_buffer_free(&after);
    return status;
  }
  case 8: {
    check_visibility value = {0};
    require(check_visibility_init(&value) == srl_ok, "Init failed");
    srl_buffer before = {0}, after = {0};
    require(check_visibility_encode(&value, srl_json, &before) == srl_ok, "Encode failed");
    srl_status status = check_visibility_decode(&value, input->data, input->size, protocol, limits);
    if (status == srl_ok) {
      status = check_visibility_encode(&value, srl_json, result);
    } else {
      require(check_visibility_encode(&value, srl_json, &after) == srl_ok, "Encode failed");
      require(before.size == after.size && !memcmp(before.data, after.data, before.size),
              "Failure changed destination");
    }
    check_visibility_free(&value);
    srl_buffer_free(&before);
    srl_buffer_free(&after);
    return status;
  }
  case 9: {
    check_default_value value = {0};
    require(check_default_value_init(&value) == srl_ok, "Init failed");
    srl_buffer before = {0}, after = {0};
    require(check_default_value_encode(&value, srl_json, &before) == srl_ok, "Encode failed");
    srl_status status =
        check_default_value_decode(&value, input->data, input->size, protocol, limits);
    if (status == srl_ok) {
      status = check_default_value_encode(&value, srl_json, result);
    } else {
      require(check_default_value_encode(&value, srl_json, &after) == srl_ok, "Encode failed");
      require(before.size == after.size && !memcmp(before.data, after.data, before.size),
              "Failure changed destination");
    }
    check_default_value_free(&value);
    srl_buffer_free(&before);
    srl_buffer_free(&after);
    return status;
  }
  default:
    require(false, "Unknown type");
    return srl_invalid;
  }
}
/* Fail each decode/encode allocation in turn and verify rollback and complete cleanup. */
static void allocation_failures(void) {
  static const char input[] =
      "{\"display_name\":\"payload\",\"labels\":[\"first\",\"second\"],\"children\":[{\"note\":"
      "\"child\"}],\"counts\":[{\"key\":\"a\",\"value\":1},{\"key\":\"a\",\"value\":2}],"
      "\"indexed\":[{\"key\":1,\"value\":{\"note\":\"map child\"}}]}";
  interop_message value = {0};
  require(interop_message_init(&value) == srl_ok, "Init failed");
  srl_buffer original = {0};
  require(interop_message_encode(&value, srl_binary_none, &original) == srl_ok, "Encode failed");
  const size_t baseline = live_allocations;
  bool succeeded = false;
  for (size_t budget = 0; budget < 256; ++budget) {
    fail_after = budget;
    const srl_status status =
        interop_message_decode(&value, (const uint8_t*)input, sizeof(input) - 1, srl_json, NULL);
    fail_after = SIZE_MAX;
    if (status == srl_ok) {
      succeeded = true;
      break;
    }
    require(status == srl_allocation, "Unexpected allocation-failure status");
    require(live_allocations == baseline, "Failed decode leaked storage");
    srl_buffer actual = {0};
    require(interop_message_encode(&value, srl_binary_none, &actual) == srl_ok, "Encode failed");
    require(actual.size == original.size && !memcmp(actual.data, original.data, actual.size),
            "Failed decode changed value");
    srl_buffer_free(&actual);
  }
  require(succeeded, "Allocation failure loop never completed");
  const size_t encode_baseline = live_allocations;
  succeeded = false;
  for (size_t budget = 0; budget < 256; ++budget) {
    fail_after = budget;
    const srl_status status = interop_message_encode(&value, srl_json, &original);
    fail_after = SIZE_MAX;
    if (status == srl_ok) {
      succeeded = true;
      break;
    }
    require(status == srl_allocation && live_allocations == encode_baseline,
            "Failed encode leaked storage");
  }
  require(succeeded, "Encode allocation failure loop never completed");
  interop_message_free(&value);
  srl_buffer_free(&original);
  require(live_allocations == 0, "Owning storage leaked");
}

/* Run the entire corpus under address/undefined sanitizers when requested by the runner. */
int main(int argc, char** argv) {
  floating_defaults();
  require(argc == 3, "Usage: main <corpus> <results>");
  char path[4096];
  snprintf(path, sizeof(path), "%s/manifest.tsv", argv[1]);
  FILE* manifest = fopen(path, "r");
  require(manifest != NULL, "Manifest missing");
  snprintf(path, sizeof(path), "%s/results.txt", argv[2]);
  FILE* statuses = fopen(path, "w");
  require(statuses != NULL, "Cannot write results");
  size_t index;
  int kind, protocol;
  srl_limits limits;
  while (fscanf(manifest, "%zu %d %d %zu %zu %zu %zu", &index, &kind, &protocol, &limits.max_bytes,
                &limits.max_string_bytes, &limits.max_elements, &limits.max_depth) == 7) {
    snprintf(path, sizeof(path), "%s/%zu.bin", argv[1], index);
    srl_buffer input = read_file(path), result = {0};
    const srl_status status = decode(kind, &input, (srl_protocol)protocol, &limits, &result);
    if (status == srl_ok) {
      snprintf(path, sizeof(path), "%s/%zu.json", argv[2], index);
      FILE* file = fopen(path, "wb");
      require(file != NULL, "Cannot write result");
      require(fwrite(result.data, 1, result.size, file) == result.size && fclose(file) == 0,
              "Write failed");
    }
    fprintf(statuses, "%s\n", status == srl_ok ? "OK" : "ERR");
    srl_buffer_free(&input);
    srl_buffer_free(&result);
  }
  allocation_failures();
  require(!ferror(manifest) && fclose(manifest) == 0 && fclose(statuses) == 0, "I/O failed");
  return 0;
}
