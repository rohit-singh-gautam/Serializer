#include "schema.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fail qualification immediately when generated ownership or wire behavior changes. */
static void require(bool success, const char* message) {
  if (!success) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
  }
}

/* Decode a complete JSON object into initialized owning storage. */
static srl_status decode_json(optional_value* value, const char* json, const srl_limits* limits) {
  return optional_value_decode(value, (const uint8_t*)json, strlen(json), srl_json, limits);
}

/* Compare two independent encoded buffers without reading beyond either span. */
static bool same_bytes(const srl_buffer* left, const srl_buffer* right) {
  return left->size == right->size && (!left->size || !memcmp(left->data, right->data, left->size));
}

/* Require malformed input to preserve the complete prior destination and its ownership. */
static void reject_unchanged(optional_value* value, const uint8_t* bytes, size_t size,
                             srl_protocol protocol, const srl_limits* limits) {
  srl_buffer before = {0}, after = {0};
  require(optional_value_encode(value, srl_binary_none, &before) == srl_ok, "Snapshot failed");
  require(optional_value_decode(value, bytes, size, protocol, limits) != srl_ok,
          "Malformed or bounded input was accepted");
  require(optional_value_encode(value, srl_binary_none, &after) == srl_ok && same_bytes(&before, &after),
          "Failed decode changed the destination");
  srl_buffer_free(&before);
  srl_buffer_free(&after);
}

/* Pin raw uint8 presence tags independently of generated encoders. */
static void scalar_wire(void) {
  optional_scalar value = {0}, decoded = {0};
  srl_buffer encoded = {0};
  require(optional_scalar_init(&value) == srl_ok && !value.value.has_value, "Absent default failed");
  require(optional_scalar_encode(&value, srl_binary_none, &encoded) == srl_ok &&
          encoded.size == 1 && encoded.data[0] == 0, "Absent wire tag changed");
  value.value.has_value = true;
  value.value.value = 42;
  const uint8_t expected[] = {1, 42, 0, 0, 0};
  require(optional_scalar_encode(&value, srl_binary_none, &encoded) == srl_ok &&
          encoded.size == sizeof(expected) && !memcmp(encoded.data, expected, sizeof(expected)),
          "Present positional bytes changed");
  for (unsigned tag = 2; tag <= UINT8_MAX; ++tag) {
    const uint8_t invalid[] = {(uint8_t)tag};
    require(optional_scalar_decode(&decoded, invalid, sizeof(invalid), srl_binary_none, NULL) != srl_ok,
            "Noncanonical presence tag accepted");
  }
  require(optional_scalar_decode(&decoded, expected, sizeof(expected), srl_binary_none, NULL) == srl_ok &&
          decoded.value.has_value && decoded.value.value == 42, "Independent present bytes rejected");
  const uint8_t absent[] = {0};
  require(optional_scalar_decode(&decoded, absent, sizeof(absent), srl_binary_none, NULL) == srl_ok &&
          !decoded.value.has_value, "Absent did not replace present");
  optional_scalar_free(&value);
  optional_scalar_free(&decoded);
  srl_buffer_free(&encoded);
}

/* Qualify nested owning records, sequences, maps, enums and every native protocol. */
int main(void) {
  scalar_wire();
  optional_value value = {0};
  const char* populated = "{\"scalar\":[7],\"record\":[{\"name\":\"kept\",\"numbers\":[1,2]}],"
    "\"nested\":[[]],\"list\":[[[],[\"x\"],[\"\"]]],"
    "\"mapping\":[[{\"key\":\"a\",\"value\":[{\"name\":\"owned\"}]}]],"
    "\"state\":[\"ready\"],\"states\":[[\"ready\"],[]]}";
  require(decode_json(&value, populated, NULL) == srl_ok, "Nested optional JSON failed");
  require(value.scalar.has_value && value.scalar.value == 7 && value.record.has_value &&
          value.nested.has_value && !value.nested.value.has_value && value.list.has_value &&
          value.list.value.size == 3 && !value.list.value.data[0].has_value &&
          value.list.value.data[1].has_value && value.list.value.data[1].value.size == 1 &&
          value.list.value.data[2].has_value && value.list.value.data[2].value.size == 0 &&
          value.mapping.has_value && value.mapping.value.size == 1 &&
          value.mapping.value.data[0].value.has_value && value.state.has_value &&
          value.state.value == optional_state_ready, "Nested presence states collapsed");
  value.cache.has_value = true;
  require(srl_string_set_cstr(&value.cache.value, "memory only") == srl_ok, "Transient setup failed");
  for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
    srl_buffer encoded = {0}, canonical = {0}, decoded_bytes = {0};
    optional_value copy = {0};
    require(optional_value_encode(&value, (srl_protocol)protocol, &encoded) == srl_ok,
            "Optional output failed");
    require(optional_value_decode(&copy, encoded.data, encoded.size, (srl_protocol)protocol, NULL) == srl_ok &&
            !copy.cache.has_value, "Roundtrip or transient reset failed");
    require(optional_value_encode(&value, srl_binary_none, &canonical) == srl_ok &&
            optional_value_encode(&copy, srl_binary_none, &decoded_bytes) == srl_ok &&
            same_bytes(&canonical, &decoded_bytes), "Optional protocol values differ");
    for (size_t length = 0; length < encoded.size; ++length) {
      reject_unchanged(&value, encoded.data, length, (srl_protocol)protocol, NULL);
    }
    require(srl_reserve(&encoded, 1), "Trailing input setup failed");
    encoded.data[encoded.size++] = 0;
    reject_unchanged(&value, encoded.data, encoded.size, (srl_protocol)protocol, NULL);
    optional_value_free(&copy);
    srl_buffer_free(&encoded);
    srl_buffer_free(&canonical);
    srl_buffer_free(&decoded_bytes);
  }
  const char* invalid[] = {"{\"scalar\":null}", "{\"scalar\":[null]}", "{\"scalar\":[1,2]}",
                            "{\"scalar\":[1,]}", "{\"nested\":[[\"x\"],[]]}"};
  for (size_t index = 0; index < sizeof(invalid)/sizeof(invalid[0]); ++index) {
    reject_unchanged(&value, (const uint8_t*)invalid[index], strlen(invalid[index]), srl_json, NULL);
  }
  srl_limits limits = srl_default_limits();
  limits.max_elements = 0;
  reject_unchanged(&value, (const uint8_t*)populated, strlen(populated), srl_json, &limits);
  limits = srl_default_limits();
  limits.max_depth = 1;
  reject_unchanged(&value, (const uint8_t*)populated, strlen(populated), srl_json, &limits);
  require(decode_json(&value, "{\"record\":[{\"name\":\"discarded\",\"numbers\":[1]}],"
                             "\"record\":[{\"numbers\":[9]}]}", NULL) == srl_ok &&
          value.record.has_value && srl_string_equal(&value.record.value.name, "fresh") &&
          value.record.value.numbers.size == 1 && value.record.value.numbers.data[0] == 9,
          "Repeated present record merged instead of replacing");
  require(decode_json(&value, "{\"mapping\":[[{\"key\":\"a\",\"value\":[{\"name\":\"old\"}]},"
                             "{\"key\":\"a\",\"value\":[{\"numbers\":[3]}]}]]}", NULL) == srl_ok &&
          value.mapping.value.size == 1 && srl_string_equal(&value.mapping.value.data[0].value.value.name, "fresh"),
          "Duplicate map keys did not replace the complete optional value");
  require(decode_json(&value, "{\"record\":[{}],\"record\":[]}", NULL) == srl_ok && !value.record.has_value,
          "Repeated absent optional did not reset ownership");
  require(decode_json(&value, "{}", NULL) == srl_ok && !value.scalar.has_value && !value.list.has_value &&
          !value.mapping.has_value && !value.cache.has_value, "Missing optionals did not use absent defaults");
  optional_value_free(&value);
  puts("C optional qualification passed: frozen tags, four protocols, nested values, exact frames, bounds, transactional failures, replacement and ownership");
  return 0;
}
