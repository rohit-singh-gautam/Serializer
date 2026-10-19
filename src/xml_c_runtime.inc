// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
/* Optional generated XML APIs require libxml2; existing binary/JSON APIs remain independent. */
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>
#include <libxml/tree.h>
#include <libxml/xmlschemas.h>
#include <inttypes.h>

/* Share one bounded output operation across provider callbacks and nested generated records. */
typedef struct srl_xml_output { srl_writer writer; srl_limits limits; srl_xml_map_limits map_limits; srl_xml_mixed_limits mixed_limits; srl_xml_digest_limits digest_limits; size_t nodes; size_t text_bytes; const char* pending_type_uri; const char* pending_type_local; size_t framing_bytes; } srl_xml_output;
/* Optional trusted prepared validator; emitted model APIs accept it only when configured. */
typedef struct srl_xml_validation {
  srl_status (*validate)(const uint8_t* bytes, size_t size, const srl_limits* limits, void* context);
  void* context;
} srl_xml_validation;
/* Test expanded names independently from namespace prefix choices. */
static inline bool srl_xml_name(xmlNodePtr node, const char* local, const char* uri) {
  return node && node->type == XML_ELEMENT_NODE && xmlStrEqual(node->name, BAD_CAST local) &&
      strcmp(node->ns && node->ns->href ? (const char*)node->ns->href : "", uri) == 0;
}
/* XML whitespace is fixed and independent of the process locale. */
static inline bool srl_xml_space(unsigned char value) { return value == ' ' || value == '\t' || value == '\r' || value == '\n'; }
/* Ignore only comments, processing instructions and indentation between fields. */
static inline xmlNodePtr srl_xml_next(xmlNodePtr node) {
  while (node && node->type != XML_ELEMENT_NODE) { node = node->next; }
  return node;
}
/* Reject a conflicting declaration instead of overriding its encoding with the UTF-8 reader. */
static inline bool srl_xml_utf8_declaration(const uint8_t* bytes, size_t size) {
  size_t start = 0;
  if (size >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf) { start = 3; }
  if (size-start < sizeof("<?xml") || memcmp(bytes+start,"<?xml",sizeof("<?xml")-1) != 0 || !srl_xml_space(bytes[start+sizeof("<?xml")-1])) { return true; }
  size_t end = start;
  while (end+1 < size && !(bytes[end] == '?' && bytes[end+1] == '>')) { ++end; }
  if (end+1 >= size) { return false; }
  for (size_t cursor = start; cursor < end; ++cursor) {
    if (end-cursor < sizeof("encoding")-1 || memcmp(bytes+cursor,"encoding",sizeof("encoding")-1) != 0) { continue; }
    cursor += sizeof("encoding")-1;
    while (cursor < end && srl_xml_space(bytes[cursor])) { ++cursor; }
    if (cursor == end || bytes[cursor++] != '=') { return false; }
    while (cursor < end && srl_xml_space(bytes[cursor])) { ++cursor; }
    if (cursor == end || (bytes[cursor] != '\'' && bytes[cursor] != '"')) { return false; }
    const uint8_t quote = bytes[cursor++];
    const size_t value = cursor;
    while (cursor < end && bytes[cursor] != quote) { ++cursor; }
    return cursor < end && cursor-value == sizeof("UTF-8")-1 &&
        (bytes[value] == 'U' || bytes[value] == 'u') && (bytes[value+1] == 'T' || bytes[value+1] == 't') &&
        (bytes[value+2] == 'F' || bytes[value+2] == 'f') && bytes[value+3] == '-' && bytes[value+4] == '8';
  }
  return true;
}
/* Reject DTD/entity/version/resource violations before constructing a provider DOM. */
static inline srl_status srl_xml_preflight(const uint8_t* bytes, size_t size, const srl_limits* limits) {
  if (size > limits->max_bytes || size > INT_MAX) { return srl_limit; }
  if (!srl_utf8(bytes, size) || !srl_xml_utf8_declaration(bytes,size)) { return srl_invalid; }
  xmlTextReaderPtr reader = xmlReaderForMemory((const char*)bytes, (int)size, NULL, "UTF-8", XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!reader) { return srl_invalid; }
  srl_status result = srl_ok; size_t nodes = 0, values = 0; int status;
  while ((status = xmlTextReaderRead(reader)) == 1) {
    const int kind = xmlTextReaderNodeType(reader);
    if (!xmlStrEqual(xmlTextReaderConstXmlVersion(reader), BAD_CAST "1.0") || kind == XML_READER_TYPE_DOCUMENT_TYPE || kind == XML_READER_TYPE_ENTITY || kind == XML_READER_TYPE_ENTITY_REFERENCE) { result = srl_invalid; break; }
    if (kind == XML_READER_TYPE_ELEMENT) {
      if (++nodes > limits->max_elements || (size_t)xmlTextReaderDepth(reader) >= limits->max_depth) { result = srl_limit; break; }
      if (xmlTextReaderMoveToFirstAttribute(reader) == 1) {
        do {
          const xmlChar* value = xmlTextReaderConstValue(reader);
          const size_t count = value ? (size_t)xmlStrlen(value) : 0;
          if (count > limits->max_string_bytes || values > limits->max_bytes || count > limits->max_bytes-values || ++nodes > limits->max_elements) { result = srl_limit; break; }
          values += count;
        } while (xmlTextReaderMoveToNextAttribute(reader) == 1);
        xmlTextReaderMoveToElement(reader);
      }
    } else if (kind == XML_READER_TYPE_TEXT || kind == XML_READER_TYPE_CDATA || kind == XML_READER_TYPE_WHITESPACE || kind == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) {
      const xmlChar* value = xmlTextReaderConstValue(reader);
      const size_t count = value ? (size_t)xmlStrlen(value) : 0;
      if (count > limits->max_string_bytes || values > limits->max_bytes || count > limits->max_bytes-values) { result = srl_limit; break; }
      values += count;
    }
    if (result != srl_ok) { break; }
  }
  if (status < 0) { result = srl_invalid; }
  xmlFreeTextReader(reader); return result;
}
/* Verify that record text is indentation only; mixed content is outside the data mapping. */
static inline bool srl_xml_record(xmlNodePtr node) {
  for (xmlNodePtr child = node->children; child; child = child->next) {
    if (child->type == XML_TEXT_NODE || child->type == XML_CDATA_SECTION_NODE) {
      for (const xmlChar* p = child->content; p && *p; ++p) { if (!srl_xml_space(*p)) { return false; } }
    } else if (child->type != XML_ELEMENT_NODE && child->type != XML_COMMENT_NODE && child->type != XML_PI_NODE) { return false; }
  }
  return true;
}
/* Scalar slots cannot silently discard nested markup or attributes. */
static inline xmlChar* srl_xml_text(xmlNodePtr node) {
  if (!node || node->properties) { return NULL; }
  for (xmlNodePtr child = node->children; child; child = child->next) { if (child->type == XML_ELEMENT_NODE || child->type == XML_ENTITY_REF_NODE) { return NULL; } }
  return xmlNodeGetContent(node);
}
/* Recognize an exact optionally signed XML integer, without locale-sensitive digits. */
static inline bool srl_xml_integer_lexical(const char* text, bool positive, const char** begin, const char** end) {
  while (srl_xml_space((unsigned char)*text)) { ++text; }
  *begin = text;
  const bool negative = *text == '-';
  if (*text == '+' || *text == '-') { ++text; }
  const char* digits = text;
  while (*text >= '0' && *text <= '9') { if (positive && negative && *text != '0') { return false; } ++text; }
  if (text == digits) { return false; }
  *end = text;
  while (srl_xml_space((unsigned char)*text)) { ++text; }
  return *text == '\0';
}
/* Parse a complete signed integer and verify its exact declared storage width. */
static inline bool srl_xml_signed(const char* text, unsigned bits, int64_t* result) {
  const char *begin, *end;
  if (!srl_xml_integer_lexical(text, false, &begin, &end)) { return false; }
  errno = 0; char* parsed; const int64_t value = strtoll(begin, &parsed, 10);
  if (errno == ERANGE || parsed != end) { return false; }
  if (bits < 64 && (value < -(INT64_C(1) << (bits-1)) || value > (INT64_C(1) << (bits-1))-1)) { return false; }
  *result = value; return true;
}
/* Parse a complete unsigned integer without accepting a negative sign or narrowing. */
static inline bool srl_xml_unsigned(const char* text, unsigned bits, uint64_t* result) {
  const char *begin, *end;
  if (!srl_xml_integer_lexical(text, true, &begin, &end)) { return false; }
  errno = 0; char* parsed; const uint64_t value = strtoull(begin, &parsed, 10);
  if (errno == ERANGE || parsed != end || (bits < 64 && value > (UINT64_C(1) << bits)-1)) { return false; }
  *result = value; return true;
}
/* Decode the exact XSD Boolean lexical vocabulary, allowing surrounding XML whitespace. */
static inline bool srl_xml_boolean(const char* text, bool* result) {
  while (srl_xml_space((unsigned char)*text)) { ++text; }
  size_t size = strlen(text); while (size && srl_xml_space((unsigned char)text[size-1])) { --size; }
  if ((size == 4 && memcmp(text,"true",4) == 0) || (size == 1 && *text == '1')) { *result = true; return true; }
  if ((size == 5 && memcmp(text,"false",5) == 0) || (size == 1 && *text == '0')) { *result = false; return true; }
  return false;
}
/* Parse exact XML floating lexical forms with locale conversion isolated to this call. */
static inline bool srl_xml_float(const char* text, bool single, double* result) {
  while (srl_xml_space((unsigned char)*text)) { ++text; }
  size_t size = strlen(text); while (size && srl_xml_space((unsigned char)text[size-1])) { --size; }
  if (size == 3 && memcmp(text,"INF",3) == 0) { *result = INFINITY; return true; }
  if (size == 4 && memcmp(text,"-INF",4) == 0) { *result = -INFINITY; return true; }
  if (size == 3 && memcmp(text,"NaN",3) == 0) { *result = NAN; return true; }
  size_t cursor = size && (text[0] == '+' || text[0] == '-') ? 1 : 0, digits = 0;
  while (cursor < size && text[cursor] >= '0' && text[cursor] <= '9') { ++cursor; ++digits; }
  if (cursor < size && text[cursor] == '.') { ++cursor; while (cursor < size && text[cursor] >= '0' && text[cursor] <= '9') { ++cursor; ++digits; } }
  if (!digits) { return false; }
  if (cursor < size && (text[cursor] == 'e' || text[cursor] == 'E')) {
    ++cursor; if (cursor < size && (text[cursor] == '+' || text[cursor] == '-')) { ++cursor; }
    const size_t begin = cursor; while (cursor < size && text[cursor] >= '0' && text[cursor] <= '9') { ++cursor; }
    if (cursor == begin) { return false; }
  }
  if (cursor != size) { return false; }
  const char* decimal = localeconv()->decimal_point; const size_t decimal_size = strlen(decimal);
  if (decimal_size > SIZE_MAX-size-1) { return false; }
  char* normalized = (char*)malloc(size+decimal_size+1); if (!normalized) { return false; }
  size_t target = 0;
  for (size_t index = 0; index < size; ++index) {
    if (text[index] == '.') { memcpy(normalized+target,decimal,decimal_size); target += decimal_size; }
    else { normalized[target++] = text[index]; }
  }
  normalized[target] = '\0'; errno = 0; char* end;
  const double value = single ? (double)strtof(normalized,&end) : strtod(normalized,&end);
  /* C11 7.22.1.3 permits ERANGE for subnormal results and correctly rounded signed zero. */
  const double smallest_normal = single ? (double)FLT_MIN : DBL_MIN;
  const bool retained_tiny = fabs(value) <= smallest_normal;
  const bool good = end == normalized+target && isfinite(value) && (errno != ERANGE || retained_tiny);
  free(normalized); if (good) { *result = value; } return good;
}
/* Validate Unicode/XML characters before handing NUL-terminated text to the provider. */
static inline bool srl_xml_characters(const char* text, size_t size) {
  if (!srl_utf8((const uint8_t*)text, size)) { return false; }
  for (size_t index = 0; index < size; ++index) {
    const unsigned char c = (unsigned char)text[index];
    if (c < 32 && c != 9 && c != 10 && c != 13) { return false; }
    if (c == 0xef && size-index >= 3 && (unsigned char)text[index+1] == 0xbf && ((unsigned char)text[index+2] == 0xbe || (unsigned char)text[index+2] == 0xbf)) { return false; }
  }
  return true;
}
/* Bound provider output before appending to the independent complete-message buffer. */
static inline int srl_xml_output_write(void* context, const char* bytes, int size) {
  srl_xml_output* output = (srl_xml_output*)context;
  const size_t maximum=output->framing_bytes ? output->framing_bytes : output->limits.max_bytes;
  if (size < 0 || output->writer.buffer.size > maximum || (size_t)size > maximum-output->writer.buffer.size) { output->writer.status = srl_limit; return -1; }
  srl_put(&output->writer, bytes, (size_t)size);
  return output->writer.status == srl_ok ? size : -1;
}
/* The caller retains the independent output state after provider finalization. */
static inline int srl_xml_output_close(void* context) { (void)context; return 0; }
/* Preserve the predefined xml binding rather than inventing an illegal alias for it. */
static inline const char* srl_xml_namespace_prefix(const char* uri, const char* fallback) {
  static const char xml_namespace_uri[] = "http://www.w3.org/XML/1998/namespace";
  return strcmp(uri, xml_namespace_uri) == 0 ? "xml" : fallback;
}
/* Write scalar text or an attribute after complete-message string accounting. */
)serializer"
R"serializer(static inline bool srl_xml_emit_text(xmlTextWriterPtr writer, srl_xml_output* output, const char* local, const char* uri, const char* prefix, const char* text, size_t size) {
  if (size > output->limits.max_string_bytes || output->text_bytes > output->limits.max_bytes || size > output->limits.max_bytes-output->text_bytes || !srl_xml_characters(text,size) || strlen(text) != size) { output->writer.status = srl_invalid; return false; }
  output->text_bytes += size;
  const int result = local ? xmlTextWriterWriteAttributeNS(writer, *uri ? BAD_CAST srl_xml_namespace_prefix(uri, prefix) : NULL, BAD_CAST local, *uri ? BAD_CAST uri : NULL, BAD_CAST text) : xmlTextWriterWriteString(writer, BAD_CAST text);
  return result >= 0;
}
/* Own a trusted standalone prepared XSD 1.0 schema; no document borrows its source bytes. */
typedef struct srl_xml_schema { xmlSchemaPtr schema; } srl_xml_schema;
/* Keep provider diagnostics local; callers receive an explicit status. */
static inline void srl_xml_schema_error(void* context, const char* message, ...) { (void)context; (void)message; }
/* Reject all external schema reference constructs before preparing the provider schema. */
static inline bool srl_xml_schema_no_references(xmlNodePtr node) {
  for (; node; node = node->next) {
    if (node->type != XML_ELEMENT_NODE) { continue; }
    if (node->ns && xmlStrEqual(node->ns->href, BAD_CAST "http://www.w3.org/2001/XMLSchema") &&
        (xmlStrEqual(node->name,BAD_CAST "include") || xmlStrEqual(node->name,BAD_CAST "import") || xmlStrEqual(node->name,BAD_CAST "redefine") || xmlStrEqual(node->name,BAD_CAST "override"))) { return false; }
    if (!srl_xml_schema_no_references(node->children)) { return false; }
  }
  return true;
}
/* Release a prepared schema; zero initialization and repeated cleanup are valid. */
static inline void srl_xml_schema_free(srl_xml_schema* schema) {
  if (schema && schema->schema) { xmlSchemaFree(schema->schema); schema->schema = NULL; }
}
/* Compile trusted standalone XSD transactionally, without resolving includes or imports. */
static inline srl_status srl_xml_schema_prepare(srl_xml_schema* result, const uint8_t* bytes, size_t size, const srl_limits* limits) {
  if (!result || (size && !bytes)) { return srl_invalid; }
  const srl_limits bounds = limits ? *limits : srl_default_limits();
  const srl_status checked = srl_xml_preflight(bytes,size,&bounds); if (checked != srl_ok) { return checked; }
  xmlDocPtr document = xmlReadMemory((const char*)bytes,(int)size,NULL,"UTF-8",XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!document || !srl_xml_schema_no_references(xmlDocGetRootElement(document))) { if (document) { xmlFreeDoc(document); } return srl_invalid; }
  xmlSchemaParserCtxtPtr parser = xmlSchemaNewDocParserCtxt(document); if (!parser) { xmlFreeDoc(document); return srl_allocation; }
  xmlSchemaSetParserErrors(parser,srl_xml_schema_error,srl_xml_schema_error,NULL);
  xmlSchemaPtr prepared = xmlSchemaParse(parser); xmlSchemaFreeParserCtxt(parser); xmlFreeDoc(document);
  if (!prepared) { return srl_invalid; }
  srl_xml_schema_free(result); result->schema = prepared; return srl_ok;
}
/* Validate one complete bounded XML document using a reusable immutable prepared schema. */
static inline srl_status srl_xml_schema_validate(const uint8_t* bytes, size_t size, const srl_limits* limits, void* context) {
  const srl_xml_schema* schema = (const srl_xml_schema*)context;
  if (!schema || !schema->schema) { return srl_invalid; }
  const srl_status checked = srl_xml_preflight(bytes,size,limits); if (checked != srl_ok) { return checked; }
  xmlDocPtr document = xmlReadMemory((const char*)bytes,(int)size,NULL,"UTF-8",XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!document) { return srl_invalid; }
  xmlSchemaValidCtxtPtr validator = xmlSchemaNewValidCtxt(schema->schema);
  if (!validator) { xmlFreeDoc(document); return srl_allocation; }
  xmlSchemaSetValidErrors(validator,srl_xml_schema_error,srl_xml_schema_error,NULL);
  const int status = xmlSchemaValidateDoc(validator,document); xmlSchemaFreeValidCtxt(validator); xmlFreeDoc(document);
  return status == 0 ? srl_ok : srl_invalid;
}
/* Bind a prepared provider schema to generated runtime-XSD-enabled model operations. */
static inline srl_xml_validation srl_xml_schema_validation(srl_xml_schema* schema) {
  const srl_xml_validation result = {srl_xml_schema_validate,schema}; return result;
}
)serializer"
