// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"ctokens(
/* A native pull reader retains one current token and private generated destination storage. */
typedef struct srl_xml_input {
  xmlTextReaderPtr reader;
  srl_limits limits;
  srl_xml_map_limits map_limits;
  srl_xml_mixed_limits mixed_limits;
  srl_xml_digest_limits digest_limits;
  srl_status status;
  size_t nodes;
  size_t text_bytes;
  size_t member_attributes;
  int kind;
  bool empty_end;
  bool nil_attribute;
} srl_xml_input;

/* Charge logical decoded values before exposing provider storage to a generated consumer. */
static inline bool srl_xml_input_charge(srl_xml_input* input, const xmlChar* text) {
  const size_t count = text ? (size_t)xmlStrlen(text) : 0;
  if (count > input->limits.max_string_bytes || input->text_bytes > input->limits.max_bytes || count > input->limits.max_bytes - input->text_bytes) {
    input->status = srl_limit;
    return false;
  }
  input->text_bytes += count;
  return true;
}

/* Advance one significant event; an empty element exposes the same virtual end as a paired tag. */
static inline bool srl_xml_input_next(srl_xml_input* input) {
  if (input->status != srl_ok) { return false; }
  if (input->empty_end) {
    input->empty_end = false;
    input->kind = XML_READER_TYPE_END_ELEMENT;
    return true;
  }
  int status;
  while ((status = xmlTextReaderRead(input->reader)) == 1) {
    input->kind = xmlTextReaderNodeType(input->reader);
    const xmlChar* version = xmlTextReaderConstXmlVersion(input->reader);
    if ((version && !xmlStrEqual(version, BAD_CAST "1.0")) ||
        input->kind == XML_READER_TYPE_DOCUMENT_TYPE || input->kind == XML_READER_TYPE_ENTITY || input->kind == XML_READER_TYPE_ENTITY_REFERENCE) {
      input->status = srl_invalid;
      return false;
    }
    if (input->kind == XML_READER_TYPE_ELEMENT) {
      if (input->nodes >= input->limits.max_elements || (size_t)xmlTextReaderDepth(input->reader) >= input->limits.max_depth) {
        input->status = srl_limit;
        return false;
      }
      ++input->nodes;
      input->member_attributes = 0;
      input->nil_attribute = false;
      if (xmlTextReaderMoveToFirstAttribute(input->reader) == 1) {
        do {
          if (input->nodes >= input->limits.max_elements || !srl_xml_input_charge(input, xmlTextReaderConstValue(input->reader))) {
            input->status = srl_limit;
            xmlTextReaderMoveToElement(input->reader);
            return false;
          }
          ++input->nodes;
          if (!xmlTextReaderIsNamespaceDecl(input->reader)) { ++input->member_attributes; }
        } while (xmlTextReaderMoveToNextAttribute(input->reader) == 1);
        xmlTextReaderMoveToElement(input->reader);
      }
      input->empty_end = xmlTextReaderIsEmptyElement(input->reader) == 1;
      return true;
    }
    if (input->kind == XML_READER_TYPE_END_ELEMENT) { return true; }
    if (input->kind == XML_READER_TYPE_TEXT || input->kind == XML_READER_TYPE_CDATA ||
        input->kind == XML_READER_TYPE_WHITESPACE || input->kind == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) {
      return srl_xml_input_charge(input, xmlTextReaderConstValue(input->reader));
    }
    if (input->kind != XML_READER_TYPE_COMMENT && input->kind != XML_READER_TYPE_PROCESSING_INSTRUCTION &&
        input->kind != XML_READER_TYPE_XML_DECLARATION && input->kind != XML_READER_TYPE_DOCUMENT) {
      input->status = srl_invalid;
      return false;
    }
  }
  input->kind = XML_READER_TYPE_NONE;
  if (status < 0) { input->status = srl_invalid; }
  return input->status == srl_ok;
}

/* Construct an operation-local nonresolving UTF8/XML1.0 provider with no retained document. */
static inline srl_status srl_xml_input_init(srl_xml_input* input, const uint8_t* bytes, size_t size, const srl_limits* limits) {
  memset(input, 0, sizeof(*input));
  input->limits = *limits;
  input->map_limits = srl_xml_default_map_limits();
  input->mixed_limits = srl_xml_default_mixed_limits();
  input->digest_limits = srl_xml_default_digest_limits();
  if (size > limits->max_bytes || size > INT_MAX) { input->status = srl_limit; return input->status; }
  if (!srl_utf8(bytes, size) || !srl_xml_utf8_declaration(bytes, size)) { input->status = srl_invalid; return input->status; }
  input->reader = xmlReaderForMemory((const char*)bytes, (int)size, NULL, "UTF-8", XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!input->reader) { input->status = srl_invalid; return input->status; }
  if (xmlTextReaderSetParserProp(input->reader, XML_PARSER_LOADDTD, 0) < 0 ||
      xmlTextReaderSetParserProp(input->reader, XML_PARSER_DEFAULTATTRS, 0) < 0 ||
      xmlTextReaderSetParserProp(input->reader, XML_PARSER_SUBST_ENTITIES, 0) < 0) {
    input->status = srl_invalid;
    return input->status;
  }
  srl_xml_input_next(input);
  return input->status;
}

/* Release only the current operation's provider; owning candidate cleanup belongs to the caller. */
static inline void srl_xml_input_free(srl_xml_input* input) {
  if (input->reader) { xmlFreeTextReader(input->reader); }
  input->reader = NULL;
}

/* Resolve identity through provider expanded names, never through prefix presentation. */
static inline bool srl_xml_input_name(const srl_xml_input* input, int kind, const char* local, const char* uri) {
  if (input->status != srl_ok || input->kind != kind || !xmlStrEqual(xmlTextReaderConstLocalName(input->reader), BAD_CAST local)) { return false; }
  const xmlChar* selected = xmlTextReaderConstNamespaceUri(input->reader);
  return strcmp(selected ? (const char*)selected : "", uri) == 0;
}

/* Skip only XML formatting whitespace while preserving nonwhite mixed content for rejection. */
static inline bool srl_xml_input_space(srl_xml_input* input) {
  while (input->kind == XML_READER_TYPE_TEXT || input->kind == XML_READER_TYPE_CDATA ||
         input->kind == XML_READER_TYPE_WHITESPACE || input->kind == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) {
    const xmlChar* text = xmlTextReaderConstValue(input->reader);
    for (const xmlChar* cursor = text; cursor && *cursor; ++cursor) { if (!srl_xml_space(*cursor)) { return true; } }
    if (!srl_xml_input_next(input)) { return false; }
  }
  return input->status == srl_ok;
}

/* Match one ordered child without consuming an unrelated following field. */
static inline bool srl_xml_input_has(srl_xml_input* input, const char* local, const char* uri) {
  return srl_xml_input_space(input) && srl_xml_input_name(input, XML_READER_TYPE_ELEMENT, local, uri);
}

/* Require an exact closing token and continue far enough to discover trailing provider errors. */
static inline bool srl_xml_input_close(srl_xml_input* input, const char* local, const char* uri) {
  return srl_xml_input_space(input) && srl_xml_input_name(input, XML_READER_TYPE_END_ELEMENT, local, uri) && srl_xml_input_next(input);
}

/* Join only the active scalar and enforce its complete size across CDATA/entity provider splits. */
static inline xmlChar* srl_xml_input_text_run(srl_xml_input* input, size_t maximum, bool require_end) {
  xmlChar* result = NULL;
  size_t size = 0;
  size_t capacity = 0;
  while (input->kind == XML_READER_TYPE_TEXT || input->kind == XML_READER_TYPE_CDATA ||
         input->kind == XML_READER_TYPE_WHITESPACE || input->kind == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) {
    const xmlChar* text = xmlTextReaderConstValue(input->reader);
    const size_t count = text ? (size_t)xmlStrlen(text) : 0;
    if (size > maximum || count > maximum-size || size > input->limits.max_string_bytes || count > input->limits.max_string_bytes - size || count >= SIZE_MAX - size) {
      input->status = srl_limit;
      xmlFree(result);
      return NULL;
    }
    const size_t required = size + count + 1;
    if (required > capacity) {
      const size_t initial_capacity_bytes = 16;
      size_t next = capacity ? capacity : initial_capacity_bytes;
      while (next < required) { if (next > SIZE_MAX / 2) { next = required; break; } next *= 2; }
      xmlChar* memory = (xmlChar*)xmlRealloc(result, next);
      if (!memory) { input->status = srl_allocation; xmlFree(result); return NULL; }
      result = memory; capacity = next;
    }
    if (count) { memcpy(result + size, text, count); }
    size += count;
    result[size] = 0;
    if (!srl_xml_input_next(input)) { xmlFree(result); return NULL; }
  }
  if (require_end && input->kind != XML_READER_TYPE_END_ELEMENT) { xmlFree(result); return NULL; }
  if (!result) {
    result = xmlStrdup(BAD_CAST "");
    if (!result) { input->status = srl_allocation; }
  }
  return result;
}
/* Require complete scalar content while sharing bounded logical text assembly with mixed owners. */
static inline xmlChar* srl_xml_input_text(srl_xml_input* input) {
  return srl_xml_input_text_run(input,input->limits.max_string_bytes,true);
}

/* Read one attribute-free scalar child without a document or completed sibling nodes. */
static inline xmlChar* srl_xml_input_scalar(srl_xml_input* input, const char* local, const char* uri) {
  if (!srl_xml_input_has(input, local, uri) || input->member_attributes > (input->nil_attribute ? 1u : 0u) || !srl_xml_input_next(input)) { return NULL; }
  xmlChar* result = srl_xml_input_text(input);
  if (result && !srl_xml_input_close(input, local, uri)) { xmlFree(result); return NULL; }
  return result;
}

/* Preserve absent/nil/value states and remove only a recognized nil marker from current attributes. */
static inline char srl_xml_input_presence(srl_xml_input* input, const char* local, const char* uri, bool absent, bool nil) {
  if (!srl_xml_input_has(input, local, uri)) { return absent && input->status == srl_ok ? 'a' : '\0'; }
  xmlChar* raw = xmlTextReaderGetAttributeNs(input->reader, BAD_CAST "nil", BAD_CAST "http://www.w3.org/2001/XMLSchema-instance");
  if (!raw) { return 'v'; }
  bool is_nil = false;
  const bool valid = nil && srl_xml_boolean((const char*)raw, &is_nil);
  xmlFree(raw);
  if (!valid) { return '\0'; }
  input->nil_attribute = true;
  if (!is_nil) { return 'v'; }
  if (input->member_attributes != 1u || !srl_xml_input_next(input)) { return '\0'; }
  xmlChar* text = srl_xml_input_text(input);
  if (!text) { return '\0'; }
  const bool empty = !*text;
  xmlFree(text);
  return empty && srl_xml_input_close(input, local, uri) ? 'n' : '\0';
}

/* A complete root must be followed only by formatting/comment/PI tokens and provider EOF. */
static inline bool srl_xml_input_finish(srl_xml_input* input) {
  return srl_xml_input_space(input) && input->kind == XML_READER_TYPE_NONE;
}
)ctokens"
