// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"

/* Stop with a concise diagnostic after any I/O, provider or generated-code failure. */
static void require(bool success, const char* message) {
  if (!success) { fprintf(stderr, "%s\n", message); exit(EXIT_FAILURE); }
}
/* Read one bounded complete fixture into independently owned storage. */
static srl_buffer read_file(const char* name) {
  FILE* file = fopen(name, "rb");
  require(file != NULL, "Cannot open fixture");
  require(fseek(file, 0, SEEK_END) == 0, "Cannot seek fixture");
  const long length = ftell(file);
  require(length >= 0 && (unsigned long)length <= 16777216u, "Fixture byte limit");
  require(fseek(file, 0, SEEK_SET) == 0, "Cannot rewind fixture");
  srl_buffer result = {0};
  require(srl_reserve(&result, (size_t)length), "Fixture allocation failed");
  result.size = (size_t)length;
  require(fread(result.data, 1, result.size, file) == result.size, "Fixture read failed");
  require(fclose(file) == 0, "Fixture close failed");
  return result;
}
/* Exercise generated XML and independent positional-binary codecs over one logical object. */
int main(int argc, char** argv) {
  require(argc == 3, "Usage: xml-c INPUT.xml OUTPUT.xml");
  srl_buffer input = read_file(argv[1]), output = {0}, binary = {0};
  xsd_example_xsd_order196281168 value = {0}, copy = {0};
  require(xsd_example_xsd_order196281168_decode_xml(&value, input.data, input.size, NULL) == srl_ok, "XML decode failed");
  require(value.xsd_id == 7u && value.xsd_count == 3 && srl_string_equal(&value.xsd_customer, "R&D"), "Scalar mapping mismatch");
  require(value.xsd_tag.size == 2 && srl_string_equal(&value.xsd_tag.data[0], "A") && srl_string_equal(&value.xsd_tag.data[1], "B"), "Repeated child mapping mismatch");
  require(xsd_example_xsd_order196281168_encode_xml(&value, &output, NULL) == srl_ok, "XML encode failed");
  require(xsd_example_xsd_order196281168_decode_xml(&copy, output.data, output.size, NULL) == srl_ok && copy.xsd_id == value.xsd_id, "XML round trip failed");
  require(xsd_example_xsd_order196281168_encode(&value, srl_binary_none, &binary) == srl_ok, "Independent binary codec failed");
  const char* cdata = "<order xmlns='urn:serializer:xsd-example' id='7'><!-- <!DOCTYPE example> --><?note <!DOCTYPE example>?><customer><![CDATA[literal <!DOCTYPE sample>]]></customer><count>3</count><tag>A</tag><tag>B</tag></order>";
  xsd_example_xsd_order196281168 probe = {0};
  require(xsd_example_xsd_order196281168_decode_xml(&probe, (const uint8_t*)cdata, strlen(cdata), NULL) == srl_ok && srl_string_equal(&probe.xsd_customer, "literal <!DOCTYPE sample>"), "Legal CDATA/comment/PI rejected");
  xsd_example_xsd_order196281168_free(&probe);
  const char* invalid[] = {"<wrong/>", "<order>", "<!DOCTYPE order SYSTEM 'file:///never-read'><order/>"};
  for (size_t index = 0; index < sizeof(invalid)/sizeof(invalid[0]); ++index) {
    require(xsd_example_xsd_order196281168_decode_xml(&copy, (const uint8_t*)invalid[index], strlen(invalid[index]), NULL) != srl_ok, "Malformed XML was accepted");
    require(copy.xsd_id == 7u, "Failed XML decode changed destination");
  }
  FILE* file = fopen(argv[2], "wb");
  require(file != NULL, "Cannot create output");
  require(fwrite(output.data, 1, output.size, file) == output.size && fclose(file) == 0, "Cannot finalize output");
  xsd_example_xsd_order196281168_free(&value); xsd_example_xsd_order196281168_free(&copy);
  srl_buffer_free(&input); srl_buffer_free(&output); srl_buffer_free(&binary);
  puts("Generated C XML and binary codecs passed");
  return 0;
}
