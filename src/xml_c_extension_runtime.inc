// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"cext(
/* Independent foreign preservation bounds; complete XML limits still cover skipped payloads. */
typedef struct srl_xml_extension_limits { size_t max_slots, max_items, max_preserved_bytes; } srl_xml_extension_limits;
/* Canonical transport has its own checked physical buffer bound, separate from native value limits. */
static const size_t srl_xml_extension_transport_framing_expansion=16;
/* Borrow capabilities only for one call; preserved XML never implies semantic understanding. */
typedef struct srl_xml_extension_options { srl_xml_extension_limits limits; const char* const* understood_namespaces; size_t understood_count; const char* required_prefixes; } srl_xml_extension_options;
/* Apply the shared conservative preservation defaults. */
static inline srl_xml_extension_options srl_xml_default_extension_options(void) {
  const srl_xml_extension_options result = {{srl_xml_extension_max_slots,srl_xml_extension_max_items,srl_xml_extension_max_preserved_bytes},NULL,0,""}; return result;
}
typedef struct srl_xml_extension_model srl_xml_extension_model;
typedef struct srl_xml_extension_field { const char* uri; const char* local; bool attribute; bool repeated; const srl_xml_extension_model* (*child)(void); } srl_xml_extension_field;
typedef struct srl_xml_extension_slot { bool attribute; size_t position; const char* const* uris; size_t uri_count; unsigned policy; size_t minimum; size_t maximum; } srl_xml_extension_slot;
typedef struct srl_xml_extension_contract { const char* core; const char* const* required; size_t required_count; const srl_xml_extension_slot* slots; size_t slot_count; } srl_xml_extension_contract;
struct srl_xml_extension_model { const char* identity; const srl_xml_extension_field* fields; size_t field_count; const srl_xml_extension_contract* contract; };
typedef struct srl_ext_binding { char* prefix; char* uri; } srl_ext_binding;
typedef struct srl_ext_scope { srl_ext_binding* data; size_t size; size_t capacity; const struct srl_ext_scope* parent; } srl_ext_scope;
typedef struct srl_ext_attribute { char* uri; char* local; char* value; } srl_ext_attribute;
typedef struct srl_ext_attributes { srl_ext_attribute* data; size_t size; size_t capacity; } srl_ext_attributes;
typedef struct srl_ext_child { size_t position; size_t item; char* uri; srl_buffer xml; } srl_ext_child;
typedef struct srl_ext_owner { srl_buffer path; const char* identity; srl_ext_scope scope; srl_ext_attributes attributes; srl_ext_child* children; size_t child_count; size_t child_capacity; } srl_ext_owner;
/* Own all sidecar bytes independently from model/source storage. Do not modify its private members. */
typedef struct srl_xml_extension_state { srl_ext_owner* owners; size_t owner_count; size_t owner_capacity; size_t preserved_bytes; size_t items; } srl_xml_extension_state;
typedef struct srl_ext_output { srl_writer writer; size_t maximum; } srl_ext_output;
typedef struct srl_ext_head { char* uri; char* local; char* prefix; srl_ext_scope scope; srl_ext_attributes attributes; } srl_ext_head;
/* Report logical retained storage without exposing editable preservation payloads. */
static inline size_t srl_xml_extension_preserved_bytes(const srl_xml_extension_state* state) { return state ? state->preserved_bytes : 0; }
/* Bound caller-supplied capability metadata independently from retained foreign payload bounds. */
static inline srl_status srl_ext_options(const srl_xml_extension_options* options) {
  if (options->understood_count>options->limits.max_items || (options->understood_count && !options->understood_namespaces)) { return srl_limit; }
  size_t bytes=options->required_prefixes ? strlen(options->required_prefixes) : 0;
  if (bytes>srl_xml_extension_max_preserved_bytes) { return srl_limit; }
  for (size_t index=0; index<options->understood_count; ++index) { const char* uri=options->understood_namespaces[index]; if (!uri) { return srl_invalid; } const size_t size=strlen(uri); if (size>srl_xml_extension_max_preserved_bytes-bytes) { return srl_limit; } bytes+=size; }
  return srl_ok;
}

/* Grow bounded operation-local vectors with checked element arithmetic. */
static inline bool srl_ext_grow(void** data, size_t* capacity, size_t count, size_t width) {
  if (count <= *capacity) { return true; }
  if (!width || count > SIZE_MAX / width) { return false; }
  size_t next = *capacity ? *capacity : 4;
  while (next < count) { if (next > SIZE_MAX / 2) { next = count; break; } next *= 2; }
  if (next > SIZE_MAX / width) { next = count; }
  void* candidate = realloc(*data,next * width); if (!candidate) { return false; } *data = candidate; *capacity = next; return true;
}
/* Copy already admitted provider strings including their independent terminator. */
static inline char* srl_ext_copy(const char* value) {
  if (!value) { value = ""; } const size_t size = strlen(value); if (size == SIZE_MAX) { return NULL; }
  char* result = (char*)malloc(size+1); if (result) { memcpy(result,value,size+1); } return result;
}
/* Release one active namespace frame, never its borrowed parent. */
static inline void srl_ext_scope_free(srl_ext_scope* scope) {
  for (size_t index=0; index<scope->size; ++index) { free(scope->data[index].prefix); free(scope->data[index].uri); } free(scope->data); memset(scope,0,sizeof(*scope));
}
/* Release one independently owned attribute inventory. */
static inline void srl_ext_attributes_free(srl_ext_attributes* values) {
  for (size_t index=0; index<values->size; ++index) { free(values->data[index].uri); free(values->data[index].local); free(values->data[index].value); } free(values->data); memset(values,0,sizeof(*values));
}
/* Release all XML-only state; NULL is accepted and native model ownership is unaffected. */
static inline void srl_xml_extension_state_free(srl_xml_extension_state* state) {
  if (!state) { return; }
  for (size_t index=0; index<state->owner_count; ++index) { srl_ext_owner* owner=&state->owners[index]; srl_buffer_free(&owner->path); srl_ext_scope_free(&owner->scope); srl_ext_attributes_free(&owner->attributes);
    for (size_t child=0; child<owner->child_count; ++child) { free(owner->children[child].uri); srl_buffer_free(&owner->children[child].xml); } free(owner->children);
  } free(state->owners); free(state);
}
/* Resolve nearest lexical bindings, including the implicit immutable xml prefix. */
static inline const char* srl_ext_namespace(const srl_ext_scope* scope, const char* prefix) {
  if (!strcmp(prefix,"xml")) { return "http://www.w3.org/XML/1998/namespace"; }
  for (; scope; scope=scope->parent) { for (size_t index=scope->size; index; --index) { if (!strcmp(scope->data[index-1].prefix,prefix)) { return scope->data[index-1].uri; } } } return NULL;
}
/* Append a namespace binding transactionally to one active or retained frame. */
static inline bool srl_ext_binding_add(srl_ext_scope* scope, const char* prefix, const char* uri) {
  srl_ext_binding value={srl_ext_copy(prefix),srl_ext_copy(uri)};
  if (!value.prefix || !value.uri || !srl_ext_grow((void**)&scope->data,&scope->capacity,scope->size+1,sizeof(*scope->data))) { free(value.prefix); free(value.uri); return false; }
  scope->data[scope->size++]=value; return true;
}
/* Flatten nearest scopes once, preserving unused aliases and lexical shadowing. */
static inline bool srl_ext_scope_copy(srl_ext_scope* target, const srl_ext_scope* scope) {
  for (const srl_ext_scope* frame=scope; frame; frame=frame->parent) { for (size_t index=0; index<frame->size; ++index) {
    if (!srl_ext_namespace(target,frame->data[index].prefix) && !srl_ext_binding_add(target,frame->data[index].prefix,frame->data[index].uri)) { return false; }
  }} return srl_ext_namespace(target,"") || srl_ext_binding_add(target,"","");
}
/* Charge every retained allocation before its logical payload is copied. */
static inline srl_status srl_ext_charge(srl_xml_extension_state* state, const srl_xml_extension_options* options, size_t size) {
  if (state->preserved_bytes > options->limits.max_preserved_bytes || size > options->limits.max_preserved_bytes-state->preserved_bytes) { return srl_limit; } state->preserved_bytes+=size; return srl_ok;
}
/* Bound transport framing before growing its contiguous byte buffer. */
static inline bool srl_ext_put(srl_ext_output* output, const char* value, size_t size) {
  if (output->writer.status != srl_ok) { return false; }
  if (output->writer.buffer.size > output->maximum || size > output->maximum-output->writer.buffer.size) { output->writer.status=srl_limit; return false; }
  srl_put(&output->writer,value,size); return output->writer.status==srl_ok;
}
/* Append a trusted static framing fragment. */
static inline bool srl_ext_raw(srl_ext_output* output, const char* value) { return srl_ext_put(output,value,strlen(value)); }
/* Escape semantic characters without changing opaque QName-looking values. */
static inline bool srl_ext_escape(srl_ext_output* output, const char* value, bool attribute) {
  for (const unsigned char* current=(const unsigned char*)value; *current; ++current) { const char* escaped=NULL;
    switch (*current) { case '&': escaped="&amp;"; break; case '<': escaped="&lt;"; break; case '>': escaped="&gt;"; break; case '"': if (attribute) { escaped="&quot;"; } break; case '\r': escaped="&#13;"; break; case '\n': if (attribute) { escaped="&#10;"; } break; case '\t': if (attribute) { escaped="&#9;"; } break; default: break; }
    if (escaped ? !srl_ext_raw(output,escaped) : !srl_ext_put(output,(const char*)current,1)) { return false; }
  } return true;
}
/* Release one current start payload after its name/attributes have been written. */
static inline void srl_ext_head_free(srl_ext_head* head) { free(head->uri); free(head->local); free(head->prefix); srl_ext_scope_free(&head->scope); srl_ext_attributes_free(&head->attributes); memset(head,0,sizeof(*head)); }
)cext"
R"cext(
/* Copy only a bounded current token and its local namespace declarations from the pull provider. */
static inline srl_status srl_ext_head_read(srl_xml_input* input, srl_ext_head* head, const srl_ext_scope* parent) {
  memset(head,0,sizeof(*head)); head->scope.parent=parent;
  if (input->kind!=XML_READER_TYPE_ELEMENT) { return srl_invalid; }
  head->uri=srl_ext_copy((const char*)xmlTextReaderConstNamespaceUri(input->reader)); head->local=srl_ext_copy((const char*)xmlTextReaderConstLocalName(input->reader)); head->prefix=srl_ext_copy((const char*)xmlTextReaderConstPrefix(input->reader));
  if (!head->uri || !head->local || !head->prefix) { return srl_allocation; }
  if (xmlTextReaderMoveToFirstAttribute(input->reader)==1) { do {
    const char* value=(const char*)xmlTextReaderConstValue(input->reader);
    if (xmlTextReaderIsNamespaceDecl(input->reader)) { const char* prefix=(const char*)xmlTextReaderConstPrefix(input->reader); const char* local=(const char*)xmlTextReaderConstLocalName(input->reader);
      if (!srl_ext_binding_add(&head->scope,prefix && !strcmp(prefix,"xmlns") ? local : "",value)) { xmlTextReaderMoveToElement(input->reader); return srl_allocation; }
    } else { srl_ext_attribute attribute={srl_ext_copy((const char*)xmlTextReaderConstNamespaceUri(input->reader)),srl_ext_copy((const char*)xmlTextReaderConstLocalName(input->reader)),srl_ext_copy(value)};
      if (!attribute.uri || !attribute.local || !attribute.value || !srl_ext_grow((void**)&head->attributes.data,&head->attributes.capacity,head->attributes.size+1,sizeof(attribute))) { free(attribute.uri); free(attribute.local); free(attribute.value); xmlTextReaderMoveToElement(input->reader); return srl_allocation; } head->attributes.data[head->attributes.size++]=attribute;
    }
  } while (xmlTextReaderMoveToNextAttribute(input->reader)==1); xmlTextReaderMoveToElement(input->reader); } return srl_ok;
}
/* Generate a prefix absent from every original binding, leaving lexical foreign values untouched. */
static inline bool srl_ext_prefix(char* result, size_t capacity, size_t* serial, const srl_ext_scope* scope) {
  do { if (*serial==SIZE_MAX) { return false; } const int count=snprintf(result,capacity,"srlopaque%zu",(*serial)++); if (count<0 || (size_t)count>=capacity) { return false; } } while (srl_ext_namespace(scope,result)); return true;
}
/* Reuse existing compatible bindings so decoding and replay never invent accumulating opaque aliases. */
static inline const char* srl_ext_select_prefix(const srl_ext_scope* scope, const char* uri, const char* preferred, bool attribute, char* generated, size_t capacity, size_t* serial) {
  if (!*uri) { return ""; } if (!strcmp(uri,"http://www.w3.org/XML/1998/namespace")) { return "xml"; }
  const char* default_uri=srl_ext_namespace(scope,""); if (!attribute && default_uri && !strcmp(default_uri,uri)) { return ""; }
  const char* mapped=srl_ext_namespace(scope,preferred);
  if ((!attribute || *preferred) && mapped && !strcmp(mapped,uri)) { return preferred; }
  for (const srl_ext_scope* frame=scope; frame; frame=frame->parent) { for (size_t index=frame->size; index; --index) { const srl_ext_binding* value=&frame->data[index-1];
    if ((!attribute || *value->prefix) && !strcmp(value->uri,uri) && !strcmp(srl_ext_namespace(scope,value->prefix),uri)) { return value->prefix; }
  }} return srl_ext_prefix(generated,capacity,serial,scope) ? generated : NULL;
}
/* Begin an expanded-name element with complete lexical scope and collision-safe framing prefixes. */
static inline srl_status srl_ext_start(srl_ext_output* output, const srl_ext_head* head, const srl_ext_scope* scope, const srl_ext_attributes* attributes, char** lexical) {
  srl_ext_scope flat={0}; if (!srl_ext_scope_copy(&flat,scope)) { srl_ext_scope_free(&flat); return srl_allocation; }
  char generated[64]=""; size_t serial=0; const char* prefix=srl_ext_select_prefix(scope,head->uri,head->prefix,false,generated,sizeof(generated),&serial);
  if (!prefix) { srl_ext_scope_free(&flat); return srl_limit; }
  const size_t prefix_size=strlen(prefix),local_size=strlen(head->local);
  if (local_size>SIZE_MAX-prefix_size-2) { srl_ext_scope_free(&flat); return srl_limit; }
  *lexical=(char*)malloc(prefix_size+local_size+2); if (!*lexical) { srl_ext_scope_free(&flat); return srl_allocation; }
  if (*prefix) { memcpy(*lexical,prefix,prefix_size); (*lexical)[prefix_size]=':'; memcpy(*lexical+prefix_size+1,head->local,local_size+1); }
  else { memcpy(*lexical,head->local,local_size+1); }
  bool good=srl_ext_raw(output,"<") && srl_ext_raw(output,*lexical);
  for (size_t index=0; good && index<flat.size; ++index) { const srl_ext_binding* binding=&flat.data[index]; if (!strcmp(binding->prefix,"xml")) { continue; }
    if (!*head->uri && !*binding->prefix) { continue; }
    good=srl_ext_raw(output," xmlns") && (!*binding->prefix || (srl_ext_raw(output,":") && srl_ext_raw(output,binding->prefix))) && srl_ext_raw(output,"=\"") && srl_ext_escape(output,binding->uri,true) && srl_ext_raw(output,"\"");
  }
  const char* existing=srl_ext_namespace(&flat,prefix);
  if (good && (!*head->uri || !existing || strcmp(existing,head->uri))) { good=srl_ext_raw(output,*prefix ? " xmlns:" : " xmlns") && (!*prefix || srl_ext_raw(output,prefix)) && srl_ext_raw(output,"=\"") && srl_ext_escape(output,head->uri,true) && srl_ext_raw(output,"\""); }
  for (size_t index=0; good && index<attributes->size; ++index) { const srl_ext_attribute* value=&attributes->data[index]; prefix=srl_ext_select_prefix(scope,value->uri,"",true,generated,sizeof(generated),&serial);
    if (!prefix) { output->writer.status=srl_limit; good=false; break; }
    if (*value->uri && strcmp(value->uri,"http://www.w3.org/XML/1998/namespace")) { existing=srl_ext_namespace(&flat,prefix);
      if (!existing || strcmp(existing,value->uri)) { good=srl_ext_raw(output," xmlns:") && srl_ext_raw(output,prefix) && srl_ext_raw(output,"=\"") && srl_ext_escape(output,value->uri,true) && srl_ext_raw(output,"\""); }
    }
    good=good && srl_ext_raw(output," ") && (!*prefix || (srl_ext_raw(output,prefix) && srl_ext_raw(output,":"))) && srl_ext_raw(output,value->local) && srl_ext_raw(output,"=\"") && srl_ext_escape(output,value->value,true) && srl_ext_raw(output,"\"");
  } good=good && srl_ext_raw(output,">"); srl_ext_scope_free(&flat); return good ? srl_ok : output->writer.status;
}
/* Close one canonical element without retaining descendant structure. */
static inline srl_status srl_ext_end(srl_ext_output* output, const char* lexical) { return srl_ext_raw(output,"</") && srl_ext_raw(output,lexical) && srl_ext_raw(output,">") ? srl_ok : output->writer.status; }
)cext"
R"cext(
/* Validate original required prefix resolution independently from preservation permissions. */
static inline srl_status srl_ext_capabilities(const srl_xml_extension_contract* contract, const srl_ext_scope* scope, const srl_xml_extension_options* options) {
  if (!contract) { return srl_ok; }
  if (contract->slot_count>options->limits.max_slots || options->understood_count>options->limits.max_items || (options->understood_count && !options->understood_namespaces)) { return srl_limit; }
  for (size_t index=0; index<contract->required_count; ++index) { const char* uri=contract->required[index]; bool understood=!strcmp(uri,contract->core);
    for (size_t value=0; !understood && value<options->understood_count; ++value) { understood=options->understood_namespaces[value] && !strcmp(uri,options->understood_namespaces[value]); } if (!understood) { return srl_invalid; }
  }
  const char* current=options->required_prefixes ? options->required_prefixes : ""; size_t count=0;
  while (*current) { while (*current==' ' || *current=='\t' || *current=='\r' || *current=='\n') { ++current; } if (!*current) { break; }
    const char* begin=current; while (*current && *current!=' ' && *current!='\t' && *current!='\r' && *current!='\n') { ++current; }
    const size_t size=(size_t)(current-begin); if (count++>=options->limits.max_items || size>srl_xml_extension_max_preserved_bytes) { return srl_limit; }
    char* prefix=(char*)malloc(size+1); if (!prefix) { return srl_allocation; } memcpy(prefix,begin,size); prefix[size]=0;
    if (!strcmp(prefix,"xmlns") || xmlValidateNCName(BAD_CAST prefix,0)!=0) { free(prefix); return srl_invalid; }
    const char* uri=srl_ext_namespace(scope,prefix); free(prefix); if (!uri || !*uri) { return srl_invalid; }
    bool understood=!strcmp(uri,contract->core); for (size_t value=0; !understood && value<options->understood_count; ++value) { understood=options->understood_namespaces[value] && !strcmp(uri,options->understood_namespaces[value]); } if (!understood) { return srl_invalid; }
  } return srl_ok;
}
/* Match foreign vocabulary only at an exact declared boundary, keeping core/known identities closed. */
static inline size_t srl_ext_slot(const srl_xml_extension_model* model, const char* uri, const char* local, bool attribute, size_t position) {
  const srl_xml_extension_contract* contract=model->contract; if (!contract || !*uri || !strcmp(uri,contract->core) || !strcmp(uri,"urn:serializer:xml:model")) { return SIZE_MAX; }
  for (size_t index=0; index<model->field_count; ++index) { if (!strcmp(uri,model->fields[index].uri) && !strcmp(local,model->fields[index].local)) { return SIZE_MAX; } }
  for (size_t index=0; index<contract->slot_count; ++index) { const srl_xml_extension_slot* slot=&contract->slots[index]; if (slot->attribute!=attribute || (!attribute && slot->position!=position)) { continue; }
    for (size_t value=0; value<slot->uri_count; ++value) { if (!strcmp(uri,slot->uris[value])) { return index; } }
  } return SIZE_MAX;
}
/* Charge preserve and skip roots globally before traversing their bounded payloads. */
static inline srl_status srl_ext_admit(srl_xml_extension_state* state, const srl_xml_extension_model* model, size_t* counts, size_t slot, const srl_xml_extension_options* options, bool* preserve) {
  if (!model->contract || slot>=model->contract->slot_count) { return srl_invalid; } const srl_xml_extension_slot* selected=&model->contract->slots[slot];
  if (!selected->policy) { return srl_invalid; } if (state->items>=options->limits.max_items || counts[slot]>=selected->maximum) { return srl_limit; }
  ++state->items; ++counts[slot]; *preserve=selected->policy==2; return srl_ok;
}
/* Require every owner-local occurrence minimum before committing a typed candidate. */
static inline srl_status srl_ext_minimum(const srl_xml_extension_model* model, const size_t* counts) {
  if (model->contract) { for (size_t index=0; index<model->contract->slot_count; ++index) { if (counts[index]<model->contract->slots[index].minimum) { return srl_invalid; } } } return srl_ok;
}
/* Lazily create retained storage only after its complete namespace/path cost is admitted. */
static inline srl_status srl_ext_owner_get(srl_xml_extension_state* state, const srl_xml_extension_model* model, const srl_buffer* path, const srl_ext_scope* scope, const srl_xml_extension_options* options, size_t* result) {
  for (size_t index=0; index<state->owner_count; ++index) { const srl_buffer* value=&state->owners[index].path; if (value->size==path->size && (!path->size || !memcmp(value->data,path->data,path->size))) { *result=index; return srl_ok; } }
  size_t bytes=64; if (path->size>SIZE_MAX-bytes) { return srl_limit; } bytes+=path->size;
  if (!srl_ext_namespace(scope,"")) { if (bytes>SIZE_MAX-64) { return srl_limit; } bytes+=64; }
  for (const srl_ext_scope* frame=scope; frame; frame=frame->parent) { for (size_t index=0; index<frame->size; ++index) { if (srl_ext_namespace(scope,frame->data[index].prefix)!=frame->data[index].uri) { continue; } const size_t first=strlen(frame->data[index].prefix),second=strlen(frame->data[index].uri); if (first>SIZE_MAX-64 || second>SIZE_MAX-64-first || bytes>SIZE_MAX-64-first-second) { return srl_limit; } bytes+=64+first+second; } }
  srl_status status=srl_ext_charge(state,options,bytes); if (status!=srl_ok) { return status; }
  if (!srl_ext_grow((void**)&state->owners,&state->owner_capacity,state->owner_count+1,sizeof(*state->owners))) { return srl_allocation; }
  *result=state->owner_count++; srl_ext_owner* owner=&state->owners[*result]; memset(owner,0,sizeof(*owner)); owner->identity=model->identity;
  if (!srl_reserve(&owner->path,path->size) || !srl_ext_scope_copy(&owner->scope,scope)) { return srl_allocation; }
  if (path->size) { memcpy(owner->path.data,path->data,path->size); } owner->path.size=path->size; return srl_ok;
}
/* Copy one already admitted attribute into independent sidecar storage. */
static inline srl_status srl_ext_attribute_copy(srl_ext_attributes* values, const srl_ext_attribute* source) {
  srl_ext_attribute value={srl_ext_copy(source->uri),srl_ext_copy(source->local),srl_ext_copy(source->value)};
  if (!value.uri || !value.local || !value.value || !srl_ext_grow((void**)&values->data,&values->capacity,values->size+1,sizeof(value))) { free(value.uri); free(value.local); free(value.value); return srl_allocation; } values->data[values->size++]=value; return srl_ok;
}
/* Traverse foreign content directly; a skipped subtree retains only active reader/scope frames. */
static inline srl_status srl_ext_foreign(srl_xml_input* input, srl_ext_output* output, const srl_ext_scope* parent) {
  srl_ext_head head; srl_status status=srl_ext_head_read(input,&head,parent); char* lexical=NULL;
  if (status==srl_ok && output) { status=srl_ext_start(output,&head,&head.scope,&head.attributes,&lexical); }
  if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; }
  while (status==srl_ok && input->kind!=XML_READER_TYPE_END_ELEMENT) {
    if (input->kind==XML_READER_TYPE_ELEMENT) { status=srl_ext_foreign(input,output,&head.scope); }
    else if (input->kind==XML_READER_TYPE_NONE) { status=srl_invalid; }
    else { if (output && !srl_ext_escape(output,(const char*)xmlTextReaderConstValue(input->reader),false)) { status=output->writer.status; }
      if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; }
    }
  }
  if (status==srl_ok && output) { status=srl_ext_end(output,lexical); }
  if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; }
  free(lexical); srl_ext_head_free(&head); return status;
}
/* Filter an ordinary typed owner while recording exact static-field/repeated-item positions. */
static inline srl_status srl_ext_filter_owner(srl_xml_input* input, srl_ext_output* output, const srl_xml_extension_model* model, srl_writer* path, srl_xml_extension_state* state, const srl_xml_extension_options* options, const srl_ext_scope* parent) {
  srl_ext_head head; srl_status status=srl_ext_head_read(input,&head,parent); char* lexical=NULL; size_t position=0;
  const size_t original_owners=state->owner_count;
  const size_t slot_count=model->contract ? model->contract->slot_count : 0;
  size_t* counts=(size_t*)calloc(slot_count ? slot_count : 1,sizeof(size_t)); size_t* items=(size_t*)calloc(model->field_count ? model->field_count : 1,sizeof(size_t));
  srl_ext_attributes known={0}; if (!counts || !items) { status=srl_allocation; }
  if (status==srl_ok) { status=srl_ext_capabilities(model->contract,&head.scope,options); }
  for (size_t index=0; status==srl_ok && index<head.attributes.size; ++index) { srl_ext_attribute* attribute=&head.attributes.data[index]; bool recognized=!strcmp(attribute->uri,"urn:serializer:xml:model");
    for (size_t field=0; !recognized && field<model->field_count; ++field) { recognized=model->fields[field].attribute && !strcmp(attribute->uri,model->fields[field].uri) && !strcmp(attribute->local,model->fields[field].local); }
    if (recognized) { if (!srl_ext_grow((void**)&known.data,&known.capacity,known.size+1,sizeof(*known.data))) { status=srl_allocation; break; } known.data[known.size++]=*attribute; }
    else { const size_t slot=srl_ext_slot(model,attribute->uri,attribute->local,true,0); bool preserve=false; status=srl_ext_admit(state,model,counts,slot,options,&preserve);
      if (status==srl_ok && preserve) { size_t owner=0; status=srl_ext_owner_get(state,model,&path->buffer,&head.scope,options,&owner);
        const size_t a=strlen(attribute->uri),b=strlen(attribute->local),c=strlen(attribute->value);
        if (status==srl_ok && (a>SIZE_MAX-48 || b>SIZE_MAX-48-a || c>SIZE_MAX-48-a-b)) { status=srl_limit; }
        if (status==srl_ok) { status=srl_ext_charge(state,options,48+a+b+c); }
        if (status==srl_ok) { status=srl_ext_attribute_copy(&state->owners[owner].attributes,attribute); }
      }
    }
  }
  if (status==srl_ok) { status=srl_ext_start(output,&head,&head.scope,&known,&lexical); } free(known.data);
  if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; }
  while (status==srl_ok && input->kind!=XML_READER_TYPE_END_ELEMENT) {
    if (input->kind==XML_READER_TYPE_ELEMENT) { const char* uri=(const char*)xmlTextReaderConstNamespaceUri(input->reader); if (!uri) { uri=""; } const char* local=(const char*)xmlTextReaderConstLocalName(input->reader); size_t selected=SIZE_MAX;
      for (size_t index=position; index<model->field_count; ++index) { const srl_xml_extension_field* field=&model->fields[index]; if (!field->attribute && !strcmp(uri,field->uri) && !strcmp(local,field->local)) { selected=index; break; } }
      if (selected!=SIZE_MAX) { const srl_xml_extension_field* field=&model->fields[selected]; const size_t item=items[selected]++; position=field->repeated ? selected : selected+1;
        if (field->child) { const size_t rollback=path->buffer.size; srl_put(path,&selected,sizeof(selected)); srl_put(path,&item,sizeof(item)); status=path->status;
          if (status==srl_ok) { status=srl_ext_filter_owner(input,output,field->child(),path,state,options,&head.scope); } path->buffer.size=rollback;
        } else { status=srl_ext_foreign(input,output,&head.scope); }
      } else { size_t slot=SIZE_MAX; for (size_t index=position; index<=model->field_count; ++index) { if (index<model->field_count && model->fields[index].attribute) { continue; } slot=srl_ext_slot(model,uri,local,false,index); if (slot!=SIZE_MAX) { position=index; break; } }
        bool preserve=false; status=srl_ext_admit(state,model,counts,slot,options,&preserve);
        if (status==srl_ok && preserve) { size_t owner=0; status=srl_ext_owner_get(state,model,&path->buffer,&head.scope,options,&owner); const size_t uri_size=strlen(uri); char* copied_uri=NULL; srl_ext_output captured={0};
          if (status==srl_ok) { status=uri_size>SIZE_MAX-24 ? srl_limit : srl_ext_charge(state,options,24+uri_size); }
          if (status==srl_ok) { copied_uri=srl_ext_copy(uri); if (!copied_uri) { status=srl_allocation; } }
          captured.maximum=options->limits.max_preserved_bytes-state->preserved_bytes;
          if (status==srl_ok) { status=srl_ext_foreign(input,&captured,&head.scope); }
          if (status==srl_ok) { status=srl_ext_charge(state,options,captured.writer.buffer.size); }
          if (status==srl_ok) { srl_ext_owner* target=&state->owners[owner]; if (!srl_ext_grow((void**)&target->children,&target->child_capacity,target->child_count+1,sizeof(*target->children))) { status=srl_allocation; }
            else { const srl_ext_child child={position,position<model->field_count ? items[position] : 0,copied_uri,captured.writer.buffer}; target->children[target->child_count++]=child; copied_uri=NULL; memset(&captured.writer.buffer,0,sizeof(captured.writer.buffer)); }
          } free(copied_uri); srl_buffer_free(&captured.writer.buffer);
        } else if (status==srl_ok) { status=srl_ext_foreign(input,NULL,&head.scope); }
      }
    } else if (input->kind==XML_READER_TYPE_NONE) { status=srl_invalid; }
    else { if (!srl_ext_escape(output,(const char*)xmlTextReaderConstValue(input->reader),false)) { status=output->writer.status; } if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; } }
  }
  if (status==srl_ok && state->owner_count>original_owners) { size_t context_owner=0; status=srl_ext_owner_get(state,model,&path->buffer,&head.scope,options,&context_owner); }
  if (status==srl_ok) { status=srl_ext_minimum(model,counts); } if (status==srl_ok) { status=srl_ext_end(output,lexical); } if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; }
  free(counts); free(items); free(lexical); srl_ext_head_free(&head); return status;
}
)cext"
R"cext(
/* Emit complete retained roots at the exact original field/item boundary. */
static inline srl_status srl_ext_replay_children(srl_ext_output* output, const srl_ext_owner* owner, size_t* cursor, size_t index, const size_t* items, const srl_xml_extension_model* model, size_t* counts, srl_xml_extension_state* admission, const srl_xml_extension_options* options) {
  if (!owner) { return srl_ok; }
  while (*cursor<owner->child_count) { const srl_ext_child* child=&owner->children[*cursor]; if (child->position>model->field_count) { return srl_invalid; } if (child->position>index) { break; }
    const size_t count=child->position<model->field_count ? items[child->position] : 0; if (child->position==index && child->item>count) { break; } if (child->item!=count) { return srl_invalid; }
    const size_t slot=srl_ext_slot(model,child->uri,"",false,child->position); bool preserve=false; srl_status status=srl_ext_admit(admission,model,counts,slot,options,&preserve); if (status!=srl_ok || !preserve) { return status==srl_ok ? srl_invalid : status; }
    if (!srl_ext_put(output,(const char*)child->xml.data,child->xml.size)) { return output->writer.status; } ++*cursor;
  } return srl_ok;
}
/* Replay borrowed sidecar payloads while traversing the generated codec's known XML stream. */
static inline srl_status srl_ext_replay_owner(srl_xml_input* input, srl_ext_output* output, const srl_xml_extension_model* model, srl_writer* path, const srl_xml_extension_state* state, bool* used, srl_xml_extension_state* admission, const srl_xml_extension_options* options, const srl_ext_scope* parent) {
  const srl_ext_owner* owner=NULL;
  for (size_t index=0; index<state->owner_count; ++index) { const srl_buffer* stored=&state->owners[index].path; if (stored->size==path->buffer.size && (!stored->size || !memcmp(stored->data,path->buffer.data,stored->size))) { owner=&state->owners[index]; used[index]=true; break; } }
  if (owner && strcmp(owner->identity,model->identity)) { return srl_invalid; }
  srl_ext_head head; srl_status status=srl_ext_head_read(input,&head,parent); char* lexical=NULL; size_t cursor=0;
  srl_ext_scope overlay={0}; const srl_ext_scope* scope=&head.scope;
  // A retained owner scope is complete. Exclude canonical known-writer aliases that were absent from original opaque XML.
  if (owner) { overlay=owner->scope; overlay.parent=NULL; scope=&overlay; }
  const size_t slot_count=model->contract ? model->contract->slot_count : 0;
  size_t* counts=(size_t*)calloc(slot_count ? slot_count : 1,sizeof(size_t)); size_t* items=(size_t*)calloc(model->field_count ? model->field_count : 1,sizeof(size_t)); srl_ext_attributes attributes={0};
  if (!counts || !items) { status=srl_allocation; } if (status==srl_ok) { status=srl_ext_capabilities(model->contract,scope,options); }
  if (status==srl_ok) { const size_t extra=owner ? owner->attributes.size : 0; if (extra>SIZE_MAX-head.attributes.size || !srl_ext_grow((void**)&attributes.data,&attributes.capacity,head.attributes.size+extra,sizeof(*attributes.data))) { status=srl_allocation; }
    else { for (size_t index=0; index<head.attributes.size; ++index) { attributes.data[attributes.size++]=head.attributes.data[index]; } }
  }
  if (owner) { for (size_t index=0; status==srl_ok && index<owner->attributes.size; ++index) { const srl_ext_attribute* value=&owner->attributes.data[index]; const size_t slot=srl_ext_slot(model,value->uri,value->local,true,0); bool preserve=false;
    status=srl_ext_admit(admission,model,counts,slot,options,&preserve); if (status==srl_ok && !preserve) { status=srl_invalid; } if (status==srl_ok) { attributes.data[attributes.size++]=*value; }
  }}
  if (status==srl_ok) { status=srl_ext_start(output,&head,scope,&attributes,&lexical); } free(attributes.data);
  if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; }
  while (status==srl_ok && input->kind!=XML_READER_TYPE_END_ELEMENT) {
    if (input->kind==XML_READER_TYPE_ELEMENT) { const char* uri=(const char*)xmlTextReaderConstNamespaceUri(input->reader); if (!uri) { uri=""; } const char* local=(const char*)xmlTextReaderConstLocalName(input->reader); size_t selected=SIZE_MAX;
      for (size_t index=0; index<model->field_count; ++index) { const srl_xml_extension_field* field=&model->fields[index]; if (!field->attribute && !strcmp(uri,field->uri) && !strcmp(local,field->local)) { selected=index; break; } }
      if (selected==SIZE_MAX) { status=srl_invalid; break; }
      status=srl_ext_replay_children(output,owner,&cursor,selected,items,model,counts,admission,options);
      const srl_xml_extension_field* field=&model->fields[selected]; const size_t item=items[selected]++;
      if (status==srl_ok && field->child) { const size_t rollback=path->buffer.size; srl_put(path,&selected,sizeof(selected)); srl_put(path,&item,sizeof(item)); status=path->status;
        if (status==srl_ok) { status=srl_ext_replay_owner(input,output,field->child(),path,state,used,admission,options,scope); } path->buffer.size=rollback;
      } else if (status==srl_ok) { status=srl_ext_foreign(input,output,scope); }
    } else if (input->kind==XML_READER_TYPE_NONE) { status=srl_invalid; }
    else { if (!srl_ext_escape(output,(const char*)xmlTextReaderConstValue(input->reader),false)) { status=output->writer.status; } if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; } }
  }
  if (status==srl_ok) { status=srl_ext_replay_children(output,owner,&cursor,model->field_count,items,model,counts,admission,options); }
  if (status==srl_ok && owner && cursor!=owner->child_count) { status=srl_invalid; }
  if (status==srl_ok) { status=srl_ext_minimum(model,counts); } if (status==srl_ok) { status=srl_ext_end(output,lexical); } if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; }
  free(counts); free(items); free(lexical); srl_ext_head_free(&head); return status;
}
/* Admit canonical framing only after the original document has passed all logical resource limits. */
static inline srl_status srl_xml_extension_transport_limits(size_t size, const srl_limits* original, srl_limits* result) {
  if (!original || !result) { return srl_invalid; } *result=*original; result->max_bytes=size; result->max_elements=size; return srl_ok;
}
/* Bound private canonical output framing without enlarging modeled string or collection admission. */
static inline srl_status srl_xml_extension_output_limits(const srl_limits* original, srl_limits* result) {
  if (!original || !result) { return srl_invalid; } if (original->max_bytes>SIZE_MAX/srl_xml_extension_transport_framing_expansion) { return srl_limit; }
  *result=*original; result->max_bytes*=srl_xml_extension_transport_framing_expansion; return srl_ok;
}
/* Validate original input completely, filtering only admitted foreign payloads into an owned sidecar.
 * The extra known-XML byte buffer is bounded by sixteen times max_bytes; no known element tree is retained.
 * Initialized outputs are replaced only on success, and every original skipped event consumes XML limits. */
static inline srl_status srl_xml_extension_filter(const uint8_t* bytes, size_t size, const srl_limits* limits, const srl_xml_extension_options* selected_options, const srl_xml_extension_model* model, srl_buffer* known, srl_xml_extension_state** result) {
  if (!limits || !model || !known || !result || (size && !bytes)) { return srl_invalid; }
  const srl_xml_extension_options defaults=srl_xml_default_extension_options(); const srl_xml_extension_options* options=selected_options ? selected_options : &defaults;
  const srl_status options_status=srl_ext_options(options); if (options_status!=srl_ok) { return options_status; }
  srl_limits framing; const srl_status framed=srl_xml_extension_output_limits(limits,&framing); if (framed!=srl_ok) { return framed; }
  srl_xml_input input; srl_status status=srl_xml_input_init(&input,bytes,size,limits); srl_ext_output output={0}; output.maximum=framing.max_bytes;
  if (status==srl_ok && !srl_xml_input_space(&input)) { status=input.status==srl_ok ? srl_invalid : input.status; }
  srl_xml_extension_state* state=(srl_xml_extension_state*)calloc(1,sizeof(*state)); srl_writer path={0}; if (!state) { status=srl_allocation; }
  if (status==srl_ok) { status=srl_ext_filter_owner(&input,&output,model,&path,state,options,NULL); }
  if (status==srl_ok && !srl_xml_input_finish(&input)) { status=input.status==srl_ok ? srl_invalid : input.status; }
  srl_xml_input_free(&input); srl_buffer_free(&path.buffer);
  if (status!=srl_ok) { srl_buffer_free(&output.writer.buffer); srl_xml_extension_state_free(state); return status; }
  srl_buffer_free(known); *known=output.writer.buffer; srl_xml_extension_state_free(*result); *result=state; return srl_ok;
}
/* Replay immutable borrowed sidecar storage, then re-admit every final byte/event before publication. */
static inline srl_status srl_xml_extension_replay(const uint8_t* bytes, size_t size, const srl_limits* limits, const srl_xml_extension_options* selected_options, const srl_xml_extension_model* model, const srl_xml_extension_state* selected_state, srl_buffer* result) {
  if (!limits || !model || !result || (size && !bytes)) { return srl_invalid; }
  const srl_xml_extension_options defaults=srl_xml_default_extension_options(); const srl_xml_extension_options* options=selected_options ? selected_options : &defaults;
  const srl_status options_status=srl_ext_options(options); if (options_status!=srl_ok) { return options_status; }
  const srl_xml_extension_state empty={0}; const srl_xml_extension_state* state=selected_state ? selected_state : &empty;
  if (state->preserved_bytes>options->limits.max_preserved_bytes) { return srl_limit; }
  bool* used=(bool*)calloc(state->owner_count ? state->owner_count : 1,sizeof(bool)); if (!used) { return srl_allocation; }
  srl_limits framing; srl_status status=srl_xml_extension_output_limits(limits,&framing); if (status!=srl_ok || size>framing.max_bytes) { free(used); return status==srl_ok ? srl_limit : status; }
  status=srl_xml_extension_transport_limits(size,limits,&framing); if (status!=srl_ok) { free(used); return status; }
  srl_xml_input input; status=srl_xml_input_init(&input,bytes,size,&framing); srl_ext_output output={0}; output.maximum=limits->max_bytes; srl_writer path={0}; srl_xml_extension_state admission={0};
  if (status==srl_ok && !srl_xml_input_space(&input)) { status=input.status==srl_ok ? srl_invalid : input.status; }
  if (status==srl_ok) { status=srl_ext_replay_owner(&input,&output,model,&path,state,used,&admission,options,NULL); }
  if (status==srl_ok && !srl_xml_input_finish(&input)) { status=input.status==srl_ok ? srl_invalid : input.status; }
  for (size_t index=0; status==srl_ok && index<state->owner_count; ++index) { if (!used[index]) { status=srl_invalid; } }
  srl_xml_input_free(&input); srl_buffer_free(&path.buffer); free(used);
  if (status==srl_ok) { srl_xml_input complete; status=srl_xml_input_init(&complete,output.writer.buffer.data,output.writer.buffer.size,limits);
    while (status==srl_ok && complete.kind!=XML_READER_TYPE_NONE) { if (!srl_xml_input_next(&complete)) { status=complete.status; } }
    if (status==srl_ok && !srl_xml_input_finish(&complete)) { status=complete.status==srl_ok ? srl_invalid : complete.status; } srl_xml_input_free(&complete);
  }
  if (status!=srl_ok) { srl_buffer_free(&output.writer.buffer); return status; } srl_buffer_free(result); *result=output.writer.buffer; return srl_ok;
}
)cext"
