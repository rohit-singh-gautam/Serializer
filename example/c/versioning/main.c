#include "schema.h"
/* Stop with a useful diagnostic on I/O, allocation, or codec failure. */
static void require(bool success, const char* message) {
  if (!success) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
  }
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

/* Read JSON, edit a typed field, and free every temporary owning value. */
int main(int argc, char** argv) {
  require(argc == 3, "Usage: main <input.json> <output.json>");
  {
    uint16_version typed = {0};
    const uint8_t expected[] = {44, 1};
    require(uint16_version_init(&typed) == srl_ok, "Revision init");
    srl_buffer encoded = {0};
    require(uint16_version_encode(&typed, srl_binary_none, &encoded) == srl_ok, "Revision encode");
    require(encoded.size == sizeof(expected) && !memcmp(encoded.data, expected, sizeof(expected)), "Revision width");
    srl_buffer_free(&encoded);
    for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
      uint16_version copy = {0};
      require(uint16_version_encode(&typed, (srl_protocol)protocol, &encoded) == srl_ok, "Revision encode");
      require(uint16_version_decode(&copy, encoded.data, encoded.size, (srl_protocol)protocol, NULL) == srl_ok, "Revision decode");
      uint16_version_free(&copy);
      srl_buffer_free(&encoded);
    }
    uint16_version_free(&typed);
  }
  {
    uint32_version typed = {0};
    const uint8_t expected[] = {112, 17, 1, 0};
    require(uint32_version_init(&typed) == srl_ok, "Revision init");
    srl_buffer encoded = {0};
    require(uint32_version_encode(&typed, srl_binary_none, &encoded) == srl_ok, "Revision encode");
    require(encoded.size == sizeof(expected) && !memcmp(encoded.data, expected, sizeof(expected)), "Revision width");
    srl_buffer_free(&encoded);
    for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
      uint32_version copy = {0};
      require(uint32_version_encode(&typed, (srl_protocol)protocol, &encoded) == srl_ok, "Revision encode");
      require(uint32_version_decode(&copy, encoded.data, encoded.size, (srl_protocol)protocol, NULL) == srl_ok, "Revision decode");
      uint32_version_free(&copy);
      srl_buffer_free(&encoded);
    }
    uint32_version_free(&typed);
  }
  {
    uint64_version typed = {0};
    const uint8_t expected[] = {255, 255, 255, 255, 255, 255, 255, 255};
    require(uint64_version_init(&typed) == srl_ok, "Revision init");
    srl_buffer encoded = {0};
    require(uint64_version_encode(&typed, srl_binary_none, &encoded) == srl_ok, "Revision encode");
    require(encoded.size == sizeof(expected) && !memcmp(encoded.data, expected, sizeof(expected)), "Revision width");
    srl_buffer_free(&encoded);
    for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
      uint64_version copy = {0};
      require(uint64_version_encode(&typed, (srl_protocol)protocol, &encoded) == srl_ok, "Revision encode");
      require(uint64_version_decode(&copy, encoded.data, encoded.size, (srl_protocol)protocol, NULL) == srl_ok, "Revision decode");
      uint64_version_free(&copy);
      srl_buffer_free(&encoded);
    }
    uint64_version_free(&typed);
  }
  {
    float_version typed = {0};
    const uint8_t expected[] = {205, 204, 204, 61};
    require(float_version_init(&typed) == srl_ok, "Revision init");
    srl_buffer encoded = {0};
    require(float_version_encode(&typed, srl_binary_none, &encoded) == srl_ok, "Revision encode");
    require(encoded.size == sizeof(expected) && !memcmp(encoded.data, expected, sizeof(expected)), "Revision width");
    srl_buffer_free(&encoded);
    for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
      float_version copy = {0};
      require(float_version_encode(&typed, (srl_protocol)protocol, &encoded) == srl_ok, "Revision encode");
      require(float_version_decode(&copy, encoded.data, encoded.size, (srl_protocol)protocol, NULL) == srl_ok, "Revision decode");
      float_version_free(&copy);
      srl_buffer_free(&encoded);
    }
    float_version_free(&typed);
  }
  {
    double_version typed = {0};
    const uint8_t expected[] = {0, 0, 0, 0, 0, 0, 4, 64};
    require(double_version_init(&typed) == srl_ok, "Revision init");
    srl_buffer encoded = {0};
    require(double_version_encode(&typed, srl_binary_none, &encoded) == srl_ok, "Revision encode");
    require(encoded.size == sizeof(expected) && !memcmp(encoded.data, expected, sizeof(expected)), "Revision width");
    srl_buffer_free(&encoded);
    for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
      double_version copy = {0};
      require(double_version_encode(&typed, (srl_protocol)protocol, &encoded) == srl_ok, "Revision encode");
      require(double_version_decode(&copy, encoded.data, encoded.size, (srl_protocol)protocol, NULL) == srl_ok, "Revision decode");
      double_version_free(&copy);
      srl_buffer_free(&encoded);
    }
    double_version_free(&typed);
  }
  {
    dotted2_version typed = {0};
    const uint8_t expected[] = {1, 0, 10, 0};
    require(dotted2_version_init(&typed) == srl_ok, "Revision init");
    srl_buffer encoded = {0};
    require(dotted2_version_encode(&typed, srl_binary_none, &encoded) == srl_ok, "Revision encode");
    require(encoded.size == sizeof(expected) && !memcmp(encoded.data, expected, sizeof(expected)), "Revision width");
    srl_buffer_free(&encoded);
    for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
      dotted2_version copy = {0};
      require(dotted2_version_encode(&typed, (srl_protocol)protocol, &encoded) == srl_ok, "Revision encode");
      require(dotted2_version_decode(&copy, encoded.data, encoded.size, (srl_protocol)protocol, NULL) == srl_ok, "Revision decode");
      dotted2_version_free(&copy);
      srl_buffer_free(&encoded);
    }
    dotted2_version_free(&typed);
  }
  {
    dotted3_version typed = {0};
    const uint8_t expected[] = {1, 0, 10, 0, 0, 0};
    require(dotted3_version_init(&typed) == srl_ok, "Revision init");
    srl_buffer encoded = {0};
    require(dotted3_version_encode(&typed, srl_binary_none, &encoded) == srl_ok, "Revision encode");
    require(encoded.size == sizeof(expected) && !memcmp(encoded.data, expected, sizeof(expected)), "Revision width");
    srl_buffer_free(&encoded);
    for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
      dotted3_version copy = {0};
      require(dotted3_version_encode(&typed, (srl_protocol)protocol, &encoded) == srl_ok, "Revision encode");
      require(dotted3_version_decode(&copy, encoded.data, encoded.size, (srl_protocol)protocol, NULL) == srl_ok, "Revision decode");
      dotted3_version_free(&copy);
      srl_buffer_free(&encoded);
    }
    dotted3_version_free(&typed);
  }
  {
    dotted4_version typed = {0};
    const uint8_t expected[] = {1, 0, 10, 0, 0, 0, 4, 0};
    require(dotted4_version_init(&typed) == srl_ok, "Revision init");
    srl_buffer encoded = {0};
    require(dotted4_version_encode(&typed, srl_binary_none, &encoded) == srl_ok, "Revision encode");
    require(encoded.size == sizeof(expected) && !memcmp(encoded.data, expected, sizeof(expected)), "Revision width");
    srl_buffer_free(&encoded);
    for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
      dotted4_version copy = {0};
      require(dotted4_version_encode(&typed, (srl_protocol)protocol, &encoded) == srl_ok, "Revision encode");
      require(dotted4_version_decode(&copy, encoded.data, encoded.size, (srl_protocol)protocol, NULL) == srl_ok, "Revision decode");
      dotted4_version_free(&copy);
      srl_buffer_free(&encoded);
    }
    dotted4_version_free(&typed);
  }
  srl_buffer input = read_file(argv[1]), canonical = {0}, json = {0};
  srl_limits limits = srl_default_limits(); limits.read_policy = srl_read_compatible;
  example_model value = {0};
  require(example_model_decode(&value, input.data, input.size, srl_json, &limits) == srl_ok,
          "Decode failed");
  srl_buffer historical = {0}; example_model old = {0};
  require(example_model_encode(&value, srl_binary_none, &historical) == srl_ok, "Historical encode failed");
  require(example_model_decode(&old, historical.data, historical.size, srl_binary_none, &limits) == srl_ok, "Historical decode failed");
  require(old.version == 8 && srl_string_equal(&old.old_name, "Ada"), "Historical mismatch");
  require(srl_string_set(&value.name, value.old_name.data, value.old_name.size) == srl_ok, "Migration failed");
  value.version = 10;
  example_model_free(&old); srl_buffer_free(&historical);
  require(example_model_encode(&value, srl_binary_none, &canonical) == srl_ok, "Encode failed");
  for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
    srl_buffer encoded = {0}, decoded = {0};
    example_model copy = {0};
    require(example_model_encode(&value, (srl_protocol)protocol, &encoded) == srl_ok,
            "Encode failed");
    require(example_model_decode(&copy, encoded.data, encoded.size, (srl_protocol)protocol, NULL) ==
                srl_ok,
            "Decode failed");
    require(example_model_encode(&copy, srl_binary_none, &decoded) == srl_ok, "Re-encode failed");
    require(decoded.size == canonical.size && !memcmp(decoded.data, canonical.data, canonical.size),
            "Value mismatch");
    example_model_free(&copy);
    srl_buffer_free(&encoded);
    srl_buffer_free(&decoded);
  }
  require(example_model_encode(&value, srl_json, &json) == srl_ok, "Encode failed");
  FILE* output = fopen(argv[2], "wb");
  require(output != NULL, "Cannot open output");
  require(fwrite(json.data, 1, json.size, output) == json.size && fclose(output) == 0,
          "Cannot write output");
  example_model_free(&value);
  srl_buffer_free(&input);
  srl_buffer_free(&canonical);
  srl_buffer_free(&json);
  puts("Version migration and four protocols passed");
  return 0;
}
