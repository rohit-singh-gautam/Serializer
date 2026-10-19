// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"cvariant(
/* One authorized expanded xsi:type is retained only until variant branch selection. */
typedef struct srl_xml_type_identity { xmlChar* uri; xmlChar* local; bool present; } srl_xml_type_identity;
/* Release the temporary QName independently of the candidate's owning payload fields. */
static inline void srl_xml_type_free(srl_xml_type_identity* value) {
  xmlFree(value->uri); xmlFree(value->local); memset(value,0,sizeof(*value));
}
/* Resolve a complete lexical QName using the current provider scope before consuming the start tag. */
static inline bool srl_xml_input_type(srl_xml_input* input,srl_xml_type_identity* result) {
  memset(result,0,sizeof(*result));
  if (input->kind != XML_READER_TYPE_ELEMENT || input->nil_attribute) { return false; }
  xmlChar* raw=xmlTextReaderGetAttributeNs(input->reader,BAD_CAST "type",BAD_CAST "http://www.w3.org/2001/XMLSchema-instance");
  if (!raw) { return true; }
  xmlChar* text=raw; while (srl_xml_space(*text)) { ++text; }
  size_t size=(size_t)xmlStrlen(text); while (size && srl_xml_space(text[size-1u])) { --size; } text[size]=0;
  if (xmlValidateQName(text,0) != 0) { xmlFree(raw); return false; }
  xmlChar* colon=(xmlChar*)xmlStrchr(text,':');
  if (colon) {
    *colon=0;
    if (xmlStrEqual(text,BAD_CAST "xmlns")) { xmlFree(raw); return false; }
    result->uri=xmlTextReaderLookupNamespace(input->reader,text);
    result->local=xmlStrdup(colon+1);
    if (!result->uri) { xmlFree(raw); srl_xml_type_free(result); return false; }
  } else {
    result->uri=xmlTextReaderLookupNamespace(input->reader,NULL);
    if (!result->uri) { result->uri=xmlStrdup(BAD_CAST ""); }
    result->local=xmlStrdup(text);
  }
  xmlFree(raw);
  if (!result->uri || !result->local) { srl_xml_type_free(result); input->status=srl_allocation; return false; }
  if (!input->member_attributes) { srl_xml_type_free(result); return false; }
  --input->member_attributes; result->present=true; return true;
}
/* Attach one configured type marker to the payload just opened by a scalar or nested record. */
static inline bool srl_xml_output_type(xmlTextWriterPtr writer,srl_xml_output* output) {
  if (!output->pending_type_local) { return true; }
  const char* local=output->pending_type_local; const char* uri=output->pending_type_uri;
  output->pending_type_local=NULL; output->pending_type_uri=NULL;
  const size_t size=strlen(local); const size_t prefix_size=sizeof("srltype:")-1u;
  if (size > SIZE_MAX-prefix_size-1u || size+prefix_size > output->limits.max_string_bytes) { return false; }
  char* lexical=(char*)malloc(size+prefix_size+1u); if (!lexical) { output->writer.status=srl_allocation; return false; }
  if (*uri) {
    if (!srl_xml_emit_text(writer,output,"xmlns:srltype","","",uri,strlen(uri))) { free(lexical); return false; }
    memcpy(lexical,"srltype:",prefix_size); memcpy(lexical+prefix_size,local,size+1u);
  } else { memcpy(lexical,local,size+1u); }
  const bool good=srl_xml_emit_text(writer,output,"type","http://www.w3.org/2001/XMLSchema-instance","srlxsi",lexical,strlen(lexical));
  free(lexical); return good;
}
)cvariant"
