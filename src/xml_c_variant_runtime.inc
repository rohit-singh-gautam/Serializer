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
  srl_xml_operation* operation=srl_xml_operation_input_context(input->reader);
  if(operation&&xmlTextReaderMoveToAttributeNs(input->reader,BAD_CAST "type",BAD_CAST "http://www.w3.org/2001/XMLSchema-instance")==1){
    const xmlChar* value=xmlTextReaderConstValue(input->reader);const size_t size=value?(size_t)xmlStrlen(value):0;
    const bool admitted=srl_xml_operation_input_allocate(input,size+1u)&&srl_xml_operation_charge(operation,srl_xml_operation_work,size+1u);
    xmlTextReaderMoveToElement(input->reader);if(!admitted){input->status=srl_invalid;return false;}
  }
  xmlChar* raw=xmlTextReaderGetAttributeNs(input->reader,BAD_CAST "type",BAD_CAST "http://www.w3.org/2001/XMLSchema-instance");
  if (!raw) { return true; }
  xmlChar* text=raw; while (srl_xml_space(*text)) { ++text; }
  size_t size=(size_t)xmlStrlen(text); while (size && srl_xml_space(text[size-1u])) { --size; } text[size]=0;
  if (xmlValidateQName(text,0) != 0) { xmlFree(raw); return false; }
  xmlChar* colon=(xmlChar*)xmlStrchr(text,':');
  if (colon) {
    *colon=0;
    if (xmlStrEqual(text,BAD_CAST "xmlns")) { xmlFree(raw); return false; }
    if(!srl_xml_operation_input_allocate(input,(size_t)xmlStrlen(colon+1)+1u)){xmlFree(raw);return false;}
    result->uri=xmlTextReaderLookupNamespace(input->reader,text);
    result->local=xmlStrdup(colon+1);
    if (!result->uri) { xmlFree(raw); srl_xml_type_free(result); return false; }
  } else {
    if(!srl_xml_operation_input_allocate(input,(size_t)xmlStrlen(text)+1u)){xmlFree(raw);return false;}
    result->uri=xmlTextReaderLookupNamespace(input->reader,NULL);
    if (!result->uri) { result->uri=xmlStrdup(BAD_CAST ""); }
    result->local=xmlStrdup(text);
  }
  xmlFree(raw);
  if(operation&&result->uri&&!srl_xml_operation_input_allocate(input,(size_t)xmlStrlen(result->uri)+1u)){srl_xml_type_free(result);return false;}
  if (!result->uri || !result->local) { srl_xml_type_free(result); input->status=srl_allocation; return false; }
  if (!input->member_attributes) { srl_xml_type_free(result); return false; }
  --input->member_attributes; result->present=true; return true;
}
/* Validate root-only MustUnderstand tokens without broadening ordinary model attribute membership. */
static inline bool srl_xml_package_capabilities(srl_xml_input* input,srl_xml_operation* operation) {
  const srl_xml_package_contract* contract=operation?operation->package_contract:NULL;
  if(!contract){return true;}if(contract->vocabulary_count&&!contract->vocabularies){return false;}
  const char* mc="http://schemas.openxmlformats.org/markup-compatibility/2006";
  if(xmlTextReaderMoveToAttributeNs(input->reader,BAD_CAST "MustUnderstand",BAD_CAST mc)!=1){return true;}
  const xmlChar* source=xmlTextReaderConstValue(input->reader);const size_t size=source?(size_t)xmlStrlen(source):0;
  const bool admitted=size!=SIZE_MAX&&srl_xml_operation_input_allocate(input,size+1u)&&srl_xml_operation_charge(operation,srl_xml_operation_work,size+1u);
  xmlChar* raw=admitted?xmlStrdup(source?source:BAD_CAST ""):NULL;xmlTextReaderMoveToElement(input->reader);
  if(!admitted||!raw){if(admitted){input->status=srl_allocation;}return false;}
  xmlChar* cursor=raw;size_t count=0;bool good=true;
  while(good&&*cursor){while(srl_xml_space(*cursor)){++cursor;}if(!*cursor){break;}xmlChar* token=cursor;
    while(*cursor&&!srl_xml_space(*cursor)){++cursor;}const xmlChar saved=*cursor;*cursor=0;
    if(++count>input->limits.max_elements||xmlValidateNCName(token,0)!=0||xmlStrEqual(token,BAD_CAST "xmlns")){good=false;}
    xmlChar* uri=good?xmlTextReaderLookupNamespace(input->reader,token):NULL;bool understood=false;
    if(uri&&*uri){for(size_t index=0;index<contract->vocabulary_count;++index){
      if(!srl_xml_operation_charge(operation,srl_xml_operation_work,1u)){good=false;break;}
      if(contract->vocabularies[index]&&!strcmp((const char*)uri,contract->vocabularies[index])){understood=true;break;}
    }}else{good=false;}xmlFree(uri);good=good&&understood;*cursor=saved;
  }
  xmlFree(raw);if(!good||!input->member_attributes){return false;}--input->member_attributes;return true;
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
/* Emit a configured type marker with explicit native quota and temporary-storage admission. */
static inline bool srl_xml_operation_output_type(xmlTextWriterPtr writer,srl_xml_output* output,srl_xml_operation* operation) {
  if(!operation){return srl_xml_output_type(writer,output);}if(!output->pending_type_local){return true;}
  const char* local=output->pending_type_local;const char* uri=output->pending_type_uri;
  output->pending_type_local=NULL;output->pending_type_uri=NULL;
  const size_t size=strlen(local),prefix_size=sizeof("srltype:")-1u;
  if(size>SIZE_MAX-prefix_size-1u||size+prefix_size>output->limits.max_string_bytes){return srl_xml_operation_output_limit(output,operation);}
  if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,size+prefix_size+1u)){output->writer.status=srl_invalid;return false;}
  char* lexical=(char*)malloc(size+prefix_size+1u);if(!lexical){output->writer.status=srl_xml_operation_fail(operation,srl_allocation);return false;}
  if(*uri){if(!srl_xml_operation_emit_text(operation,writer,output,"xmlns:srltype","","",uri,strlen(uri))){free(lexical);return false;}memcpy(lexical,"srltype:",prefix_size);memcpy(lexical+prefix_size,local,size+1u);}
  else{memcpy(lexical,local,size+1u);}
  const bool good=srl_xml_operation_emit_text(operation,writer,output,"type","http://www.w3.org/2001/XMLSchema-instance","srlxsi",lexical,strlen(lexical));free(lexical);return good;
}
)cvariant"
