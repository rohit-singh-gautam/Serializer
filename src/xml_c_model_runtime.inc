// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"cmodel(
/* Independent byte admission preserves the layouts of existing XML/native limits. */
typedef struct srl_xml_digest_limits { size_t max_bytes; } srl_xml_digest_limits;
/* Apply the shared one MiB limit independently to every nested digest value. */
static inline srl_xml_digest_limits srl_xml_default_digest_limits(void) {
  const srl_xml_digest_limits result={1024u*1024u}; return result;
}
/* Check exact canonical numeric revision grammar before native range conversion. */
static inline bool srl_xml_revision_number(const char* text,bool floating) {
  size_t offset=0; while (text[offset] >= '0' && text[offset] <= '9') { ++offset; }
  if (!offset || (offset > 1 && text[0] == '0')) { return false; }
  if (floating && text[offset] == '.') {
    const size_t start=++offset; while (text[offset] >= '0' && text[offset] <= '9') { ++offset; }
    if (offset == start) { return false; }
  }
  if (floating && (text[offset] == 'e' || text[offset] == 'E')) {
    ++offset; if (text[offset] == '+' || text[offset] == '-') { ++offset; }
    const size_t start=offset; while (text[offset] >= '0' && text[offset] <= '9') { ++offset; }
    if (offset == start) { return false; }
  }
  return text[offset] == 0;
}
/* Convert one ASCII hexadecimal digit without accepting XML whitespace. */
static inline int srl_xml_digest_digit(unsigned char value) {
  if (value >= '0' && value <= '9') { return value-'0'; }
  if (value >= 'a' && value <= 'f') { return value-'a'+10; }
  if (value >= 'A' && value <= 'F') { return value-'A'+10; }
  return -1;
}
/* Admit exact hex before allocation and replace an owning buffer only on complete success. */
static inline srl_status srl_xml_digest_read(const char* text,size_t size,size_t extent,
    srl_xml_digest_limits limits,srl_buffer* result) {
  const size_t count=size/2u;
  if (size%2u || (extent && count != extent)) { return srl_invalid; }
  if (count > limits.max_bytes) { return srl_limit; }
  for (size_t index=0; index < size; ++index) { if (srl_xml_digest_digit((unsigned char)text[index]) < 0) { return srl_invalid; } }
  srl_buffer candidate={0}; candidate.data=count ? (uint8_t*)malloc(count) : NULL;
  if (count && !candidate.data) { return srl_allocation; } candidate.size=count; candidate.capacity=count;
  for (size_t index=0; index < count; ++index) { candidate.data[index]=(uint8_t)(srl_xml_digest_digit((unsigned char)text[index*2u])*16+srl_xml_digest_digit((unsigned char)text[index*2u+1u])); }
  srl_buffer_free(result); *result=candidate; return srl_ok;
}
/* Produce canonical lowercase hex from borrowed native storage after independent size admission. */
static inline srl_status srl_xml_digest_text(const srl_buffer* value,size_t extent,
    srl_xml_digest_limits limits,srl_buffer* result) {
  if ((value->size && !value->data) || (extent && value->size != extent)) { return srl_invalid; }
  if (value->size > limits.max_bytes || value->size > (SIZE_MAX-1u)/2u) { return srl_limit; }
  const size_t size=value->size*2u; result->data=(uint8_t*)malloc(size+1u); if (!result->data) { return srl_allocation; }
  result->size=size; result->capacity=size+1u; const char* hex="0123456789abcdef";
  for (size_t index=0; index < value->size; ++index) { result->data[index*2u]=(uint8_t)hex[value->data[index]>>4]; result->data[index*2u+1u]=(uint8_t)hex[value->data[index]&15u]; }
  result->data[size]=0; return srl_ok;
}
)cmodel"
