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
  srl_buffer input = read_file(argv[1]), canonical = {0}, json = {0};
  example_model value = {0};
  require(example_model_decode(&value, input.data, input.size, srl_json, NULL) == srl_ok,
          "Decode failed");
  ++value.revision;
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
  puts("Four protocols passed");
  return 0;
}
