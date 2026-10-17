// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Stop immediately if a generated model/provider contract is violated. */
static void require(bool success, const char* message) {
  if (!success) { fprintf(stderr, "%s\n", message); exit(EXIT_FAILURE); }
}
/* Qualify native typed XML mappings and both optional prepared-XSD validation directions. */
int main(void) {
  shapes_record value = {0}, copy = {0};
  require(shapes_record_init(&value) == srl_ok, "Initialization failed");
  value.big = UINT64_MAX;
  require(srl_string_set_cstr(&value.label, "\tX\r\n\xf0\x9f\x98\x80") == srl_ok, "String allocation failed");
  value.flag.has_value = true; value.flag.value = true;
  value.nested.value = INT16_MIN;
  value.children.data = (shapes_child*)calloc(1, sizeof(shapes_child));
  require(value.children.data != NULL, "Child allocation failed");
  value.children.size = value.children.capacity = 1;
  require(shapes_child_init(&value.children.data[0]) == srl_ok, "Child initialization failed");
  value.children.data[0].value = INT16_MAX;
  value.states.data[0] = shapes_state_zero; value.states.data[1] = shapes_state_one;
  value.amount = INFINITY; value.small = 1.25f;
  srl_buffer encoded = {0}, first = {0}, second = {0};
  require(shapes_record_encode_xml(&value, &encoded, NULL, NULL) == srl_ok, "XML encode failed");
  require(shapes_record_decode_xml(&copy, encoded.data, encoded.size, NULL, NULL) == srl_ok, "XML decode failed");
  require(shapes_record_encode(&value, srl_binary_none, &first) == srl_ok && shapes_record_encode(&copy, srl_binary_none, &second) == srl_ok, "Binary encode failed");
  require(first.size == second.size && memcmp(first.data, second.data, first.size) == 0, "Typed round trip failed");
  value.small = FLT_TRUE_MIN; value.amount = DBL_TRUE_MIN;
  require(shapes_record_encode_xml(&value, &encoded, NULL, NULL) == srl_ok && shapes_record_decode_xml(&copy, encoded.data, encoded.size, NULL, NULL) == srl_ok, "Subnormal XML round trip failed");
  require(copy.small == FLT_TRUE_MIN && copy.amount == DBL_TRUE_MIN, "Representable subnormal lost");
  double boundary = 0;
  require(srl_xml_float("2.2250738585072014e-308", false, &boundary) && boundary == DBL_MIN, "Smallest normal XML rejected");
  require(!srl_xml_float("1e99999", false, &boundary), "Overflowing XML accepted");
  require(!srl_xml_float("1e-99999", false, &boundary), "XML underflow to zero accepted");
  value.small = 1.25f; value.amount = INFINITY;
  value.flag.has_value = false;
  require(shapes_record_encode_xml(&value, &encoded, NULL, NULL) == srl_ok && shapes_record_decode_xml(&copy, encoded.data, encoded.size, NULL, NULL) == srl_ok && !copy.flag.has_value, "Optional absence failed");
  const char* zero = "<record big='-000'><label>x</label><nested><value>1</value></nested><states>zero</states><states>one</states><amount>INF</amount><small>1.25</small></record>";
  require(shapes_record_decode_xml(&copy, (const uint8_t*)zero, strlen(zero), NULL, NULL) == srl_ok && copy.big == 0, "Unsigned negative zero failed");
  const char* numeric_locale = setlocale(LC_NUMERIC, NULL);
  const size_t locale_bytes = strlen(numeric_locale) + 1;
  char* saved_locale = (char*)malloc(locale_bytes);
  require(saved_locale != NULL, "Locale allocation failed");
  memcpy(saved_locale, numeric_locale, locale_bytes);
  const char* candidates[] = {"German_Germany.1252", "French_France.1252", "de-DE", "fr-FR", "de_DE.UTF-8", "fr_FR.UTF-8", "de_DE", "fr_FR"};
  bool non_c_locale = false;
  for (size_t index = 0; index < sizeof(candidates)/sizeof(candidates[0]); ++index) {
    if (setlocale(LC_NUMERIC, candidates[index]) && strcmp(localeconv()->decimal_point, ".") != 0) { non_c_locale = true; break; }
  }
  if (non_c_locale) {
    value.amount = 12.5;
    require(shapes_record_encode_xml(&value, &encoded, NULL, NULL) == srl_ok, "Locale-independent XML encode failed");
    require(shapes_record_decode_xml(&copy, encoded.data, encoded.size, NULL, NULL) == srl_ok && copy.amount == 12.5 && copy.small == 1.25f, "Locale-independent XML decode failed");
    require(shapes_record_decode_xml(&copy, (const uint8_t*)zero, strlen(zero), NULL, NULL) == srl_ok && copy.big == 0 && copy.small == 1.25f, "Dot decimal rejected in non-C locale");
    puts("C XML non-C numeric locale regression passed");
  } else { puts("SKIP: no installed German/French numeric locale"); }
  require(setlocale(LC_NUMERIC, saved_locale) != NULL, "Locale restoration failed");
  free(saved_locale);
  const char* xsd = "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='record' type='xs:string'/></xs:schema>";
  srl_xml_schema schema = {0};
  require(srl_xml_schema_prepare(&schema, (const uint8_t*)xsd, strlen(xsd), NULL) == srl_ok, "XSD preparation failed");
  const srl_xml_validation validator = srl_xml_schema_validation(&schema);
  require(shapes_record_decode_xml(&copy, encoded.data, encoded.size, NULL, &validator) != srl_ok && copy.big == 0, "Input XSD ignored or destination changed");
  uint8_t* saved_data = encoded.data; const size_t saved_size = encoded.size;
  require(shapes_record_encode_xml(&value, &encoded, NULL, &validator) != srl_ok && encoded.data == saved_data && encoded.size == saved_size, "Output XSD ignored or destination changed");
  srl_xml_schema_free(&schema);
  shapes_record_free(&value); shapes_record_free(&copy);
  srl_buffer_free(&encoded); srl_buffer_free(&first); srl_buffer_free(&second);
  puts("C generated typed XML and prepared XSD checks passed");
  return 0;
}
