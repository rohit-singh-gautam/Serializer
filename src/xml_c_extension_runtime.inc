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
typedef struct srl_ext_scope srl_ext_scope;
/* Private compiler/runtime discriminants for stable map companion identities. */
static const unsigned srl_ext_map_lexical_key=1;
static const unsigned srl_ext_map_boolean_key=3;
typedef struct srl_xml_namespace_name { const char* uri; const char* local; } srl_xml_namespace_name;
typedef struct srl_xml_namespace_list { const srl_xml_namespace_name* items; size_t size; } srl_xml_namespace_list;
typedef struct srl_xml_extension_field { const char* uri; const char* local; bool attribute; bool repeated; const srl_xml_extension_model* (*child)(void); unsigned namespace_kind; const srl_xml_namespace_list* namespace_enumeration; size_t namespace_enumeration_count; const char* type_uri; const char* type_local; const srl_xml_scalar_rule* scalar_rule; } srl_xml_extension_field;
typedef struct srl_xml_extension_slot { bool attribute; size_t position; const char* const* uris; size_t uri_count; unsigned policy; size_t minimum; size_t maximum; unsigned namespace_match; unsigned process_contents; } srl_xml_extension_slot;
typedef srl_status (*srl_xml_declaration_validator)(const char*,const char*,const char*,const srl_ext_scope*,const uint8_t*,size_t,const srl_limits*,srl_xml_operation*,bool*);
typedef struct srl_xml_extension_contract { const char* core; const char* const* required; size_t required_count; const srl_xml_extension_slot* slots; size_t slot_count; srl_xml_declaration_validator validate; } srl_xml_extension_contract;
struct srl_xml_extension_model { const char* identity; const srl_xml_extension_field* fields; size_t field_count; const srl_xml_extension_contract* contract; unsigned namespace_text; const srl_xml_namespace_list* namespace_enumeration; size_t namespace_enumeration_count; bool flexible_children; bool mixed_text; unsigned map_key_kind; const srl_xml_scalar_rule* scalar_rule; };
/* Match a variant payload's expanded xsi:type without consuming its original attributes. */
static inline bool srl_ext_typed_field(srl_xml_input* input,const srl_xml_extension_field* field) {
  if (!field->type_uri) { return true; }
  xmlChar* value=xmlTextReaderGetAttributeNs(input->reader,BAD_CAST "type",BAD_CAST "http://www.w3.org/2001/XMLSchema-instance");
  if (!value) { return false; }
  xmlChar* begin=value; while (*begin==' ' || *begin=='\t' || *begin=='\r' || *begin=='\n') { ++begin; }
  size_t size=xmlStrlen(begin); while (size && (begin[size-1]==' ' || begin[size-1]=='\t' || begin[size-1]=='\r' || begin[size-1]=='\n')) { begin[--size]=0; }
  xmlChar* colon=(xmlChar*)xmlStrchr(begin,':'); const xmlChar* local=colon ? colon+1 : begin;
  if (colon) { *colon=0; }
  xmlChar* uri=xmlTextReaderLookupNamespace(input->reader,colon ? begin : NULL);
  const bool result=xmlValidateNCName(local,0)==0 && (!colon || xmlValidateNCName(begin,0)==0) &&
      !strcmp((const char*)local,field->type_local) && !strcmp(uri ? (const char*)uri : "",field->type_uri);
  xmlFree(uri); xmlFree(value); return result;
}
/* Scalar children use ordinary occurrence paths, with no application record or wire-field changes. */
static inline const srl_xml_extension_model* srl_xml_namespace_model_1(void) { static const srl_xml_extension_model model={"serializer:QName",NULL,0,NULL,1,NULL,0,false,false,0,NULL};return &model; }
/* Keep a QName list's local scope through the same scalar occurrence inventory. */
static inline const srl_xml_extension_model* srl_xml_namespace_model_2(void) { static const srl_xml_extension_model model={"serializer:QName-list",NULL,0,NULL,2,NULL,0,false,false,0,NULL};return &model; }
/* Prefix-list items retain URI identity while their original lexical prefixes remain unchanged. */
static inline const srl_xml_extension_model* srl_xml_namespace_model_3(void) { static const srl_xml_extension_model model={"serializer:prefix-list",NULL,0,NULL,3,NULL,0,false,false,0,NULL};return &model; }
typedef struct srl_ext_binding { char* prefix; char* uri; } srl_ext_binding;
struct srl_ext_scope { srl_ext_binding* data; size_t size; size_t capacity; const struct srl_ext_scope* parent; };
typedef struct srl_ext_attribute { char* uri; char* local; char* value; } srl_ext_attribute;
typedef struct srl_ext_attributes { srl_ext_attribute* data; size_t size; size_t capacity; } srl_ext_attributes;
/* Observe only a true nil marker; the typed reader remains responsible for its complete validity. */
static inline bool srl_ext_nil(const srl_ext_attributes* attributes) {
  for (size_t index=0;index<attributes->size;++index) {
    const srl_ext_attribute* attribute=&attributes->data[index];
    if (strcmp(attribute->uri,"http://www.w3.org/2001/XMLSchema-instance") || strcmp(attribute->local,"nil")) { continue; }
    const char* value=attribute->value;size_t size=strlen(value);
    while (size && srl_xml_scalar_space((unsigned char)*value)) { ++value;--size; }
    while (size && srl_xml_scalar_space((unsigned char)value[size-1])) { --size; }
    return (size==1 && *value=='1') || (size==4 && !memcmp(value,"true",4));
  }
  return false;
}
typedef struct srl_ext_child { size_t position; size_t item; char* uri; srl_buffer xml; } srl_ext_child;
typedef struct srl_ext_owner { srl_buffer path; const char* identity; srl_ext_scope scope; srl_ext_attributes attributes; srl_ext_child* children; size_t child_count; size_t child_capacity; size_t* defaults; size_t default_count,default_capacity; } srl_ext_owner;
/* Own all sidecar bytes independently from model/source storage. Do not modify its private members. */
typedef struct srl_xml_extension_state { srl_ext_owner* owners; size_t owner_count; size_t owner_capacity; size_t preserved_bytes; size_t items; bool validation_only; } srl_xml_extension_state;
typedef struct srl_ext_output { srl_writer writer; size_t maximum; const srl_limits* declaration_limits; srl_xml_operation* declaration_operation; } srl_ext_output;
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
    for (size_t child=0; child<owner->child_count; ++child) { free(owner->children[child].uri); srl_buffer_free(&owner->children[child].xml); } free(owner->children);free(owner->defaults);
  } free(state->owners); free(state);
}
/* Resolve nearest lexical bindings, including the implicit immutable xml prefix. */
static inline const char* srl_ext_namespace(const srl_ext_scope* scope, const char* prefix) {
  if (!strcmp(prefix,"xml")) { return "http://www.w3.org/XML/1998/namespace"; }
  for (; scope; scope=scope->parent) { for (size_t index=scope->size; index; --index) { if (!strcmp(scope->data[index-1].prefix,prefix)) { return scope->data[index-1].uri; } } } return NULL;
}
/* Validate namespace-dependent tokens in their own complete scope, after bounded XML admission. */
static inline srl_status srl_ext_namespace_tokens(const char* raw, unsigned kind,
    const srl_ext_scope* scope, size_t maximum_items,
    const srl_xml_namespace_list* enumeration, size_t enumeration_count) {
  if (!kind) { return srl_ok; }
  srl_string normalized={0};srl_status status=srl_xml_scalar_normalize(raw,strlen(raw),"collapse",&normalized);
  if (status!=srl_ok) { return status; }
  size_t offset=0,begin=0,size=0,count=0;
  while (status==srl_ok && srl_xml_scalar_token(normalized.data,&offset,&begin,&size)) {
    if (count==maximum_items) { status=srl_limit;break; } ++count;
    char* token=normalized.data+begin;const char saved=token[size];token[size]=0;
    char* colon=strchr(token,':');
    if (kind==3) {
      const char* uri=srl_ext_namespace(scope,token);
      if (srl_xml_scalar_lexical("NCName",token)!=srl_ok || !strcmp(token,"xmlns") || !uri || !*uri) { status=srl_invalid; }
    } else if (colon) {
      *colon=0;const char* uri=srl_ext_namespace(scope,token);
      if (srl_xml_scalar_lexical("NCName",token)!=srl_ok || !strcmp(token,"xmlns") || !uri || !*uri ||
          srl_xml_scalar_lexical("NCName",colon+1)!=srl_ok) { status=srl_invalid; } *colon=':';
    } else if (srl_xml_scalar_lexical("NCName",token)!=srl_ok) { status=srl_invalid; }
    token[size]=saved;
  }
  if (status==srl_ok && kind==1 && count!=1) { status=srl_invalid; }
  if (status==srl_ok && enumeration_count) {
    bool matched=false;
    for (size_t alternative=0; !matched && alternative<enumeration_count; ++alternative) {
      const srl_xml_namespace_list* allowed=&enumeration[alternative];
      if (allowed->size!=count) { continue; }
      offset=0;size_t index=0;bool equal=true;
      while (equal && srl_xml_scalar_token(normalized.data,&offset,&begin,&size)) {
        char* token=normalized.data+begin;const char saved=token[size];token[size]=0;
        char* colon=strchr(token,':');const char* local=colon ? colon+1 : token;
        if (colon) { *colon=0; }
        const char* uri=srl_ext_namespace(scope,colon ? token : "");
        if (!uri) { uri=""; }
        equal=!strcmp(uri,allowed->items[index].uri) && !strcmp(local,allowed->items[index].local);
        if (colon) { *colon=':'; } token[size]=saved;++index;
      }
      matched=equal;
    }
    if (!matched) { status=srl_invalid; }
  }
  srl_string_free(&normalized);return status;
}
/* Forward scoped member selection for ordered list-item identity comparisons. */
static inline srl_status srl_ext_scalar_selected(const srl_xml_scalar_rule*,const char*,const srl_ext_scope*,srl_xml_scalar_work*,srl_string*,const srl_xml_scalar_rule**);
/* Compare a selected primitive identity with immutable schema-owned expanded names or values. */
static inline srl_status srl_ext_scalar_identity(const srl_xml_scalar_rule* selected,const char* text,
    const srl_xml_scalar_identity* identity,const srl_ext_scope* scope,srl_xml_scalar_work* work,bool* equal) {
  *equal=false;srl_xml_scalar_rule expected={0};expected.kind=identity->kind;expected.list_kind=identity->list_kind;
  if (!srl_xml_scalar_same_family(selected,&expected)) { return srl_ok; })cext"
R"cext(
  if(*selected->list_kind&&(identity->item_count||!identity->name_count)){
    size_t count=0;srl_status status=srl_xml_scalar_count(text,work->max_items,&count);if(status!=srl_ok||count!=identity->item_count){return status;}
    size_t offset=0,begin=0,size=0,index=0;bool matched=true;
    srl_xml_scalar_rule builtin={0};builtin.kind=selected->list_kind;builtin.whitespace="collapse";builtin.list_kind="";
    builtin.length=builtin.minimum=builtin.maximum=SIZE_MAX;for(size_t bound=0;bound<sizeof(builtin.bounds)/sizeof(builtin.bounds[0]);++bound){builtin.bounds[bound]="";}
    while(status==srl_ok&&matched&&srl_xml_scalar_token(text,&offset,&begin,&size)){
      srl_string token={0},checked={0};const srl_xml_scalar_rule* item=NULL;status=srl_string_set(&token,text+begin,size);
      if(status==srl_ok){status=srl_ext_scalar_selected(selected->item_rule?selected->item_rule:&builtin,token.data,scope,work,&checked,&item);}
      if(status==srl_ok){status=srl_ext_scalar_identity(item,checked.data,&identity->items[index++],scope,work,&matched);}
      srl_string_free(&token);srl_string_free(&checked);
    }*equal=status==srl_ok&&matched;return status;
  }
  if (!strcmp(*selected->list_kind ? selected->list_kind : selected->kind,"QName")) {
    size_t offset=0,begin=0,size=0,index=0;bool matched=true;
    srl_string copy={0};srl_status status=srl_string_set(&copy,text,strlen(text));if(status!=srl_ok){return status;}
    while(matched && srl_xml_scalar_token(copy.data,&offset,&begin,&size)) {
      if(index==identity->name_count){matched=false;break;}
      char* token=copy.data+begin;const char saved=token[size];token[size]=0;char* colon=strchr(token,':');
      const char* local=colon ? colon+1 : token;if(colon){*colon=0;}const char* uri=srl_ext_namespace(scope,colon ? token : "");
      matched=!strcmp(uri ? uri : "",identity->names[index].uri)&&!strcmp(local,identity->names[index].local);
      if(colon){*colon=':';}token[size]=saved;++index;
    }
    *equal=matched&&index==identity->name_count;srl_string_free(&copy);return srl_ok;
  }
  int order=1;srl_status status=srl_xml_scalar_compare(selected,text,identity->lexical,true,work,&order);
  *equal=status==srl_ok&&!order;return status;
}
/* Select independent union members only after each QName resolves in its actual occurrence scope. */
static inline srl_status srl_ext_scalar_selected(const srl_xml_scalar_rule* rule,const char* raw,
    const srl_ext_scope* scope,srl_xml_scalar_work* work,srl_string* result,const srl_xml_scalar_rule** selected_leaf) {
  const srl_xml_scalar_rule* selected=rule;srl_string text={0};srl_status status=srl_invalid;
  if(rule->member_count) {
    for(size_t index=0;index<rule->member_count;++index) {
      status=srl_ext_scalar_selected(&rule->members[index],raw,scope,work,&text,&selected);
      if(status==srl_ok||status==srl_limit||status==srl_allocation){break;}
    }
    if(status!=srl_ok){srl_string_free(&text);return status;}
  }
  srl_xml_scalar_rule outer=*rule;
  if(rule->member_count){outer.kind="";outer.whitespace="preserve";outer.members=NULL;outer.member_count=0;}
  if(rule->identity_count){outer.enumeration=NULL;outer.enumeration_count=0;}
  if(rule->fixed_identity){outer.fixed=NULL;}
  outer.identities=NULL;outer.identity_count=0;outer.fixed_identity=NULL;outer.item_rule=NULL;
  srl_string checked={0};status=srl_xml_scalar_check_selected(&outer,rule->member_count ? text.data : raw,
      rule->member_count ? text.size : strlen(raw),work,&checked,NULL);srl_string_free(&text);text=checked;
  if(status==srl_ok&&rule->item_rule){
    size_t offset=0,begin=0,size=0;while(status==srl_ok&&srl_xml_scalar_token(text.data,&offset,&begin,&size)){
      srl_string token={0},item={0};status=srl_string_set(&token,text.data+begin,size);
      if(status==srl_ok){status=srl_ext_scalar_selected(rule->item_rule,token.data,scope,work,&item,NULL);}
      srl_string_free(&token);srl_string_free(&item);
    }
  }
  if(status==srl_ok && !rule->member_count && (!strcmp(rule->kind,"QName")||!strcmp(rule->list_kind,"QName"))) {
    status=srl_ext_namespace_tokens(text.data,*rule->list_kind ? 2u : 1u,scope,work->max_items,NULL,0);
  }
  if(status==srl_ok&&rule->identity_count) {
    bool matched=false;for(size_t index=0;status==srl_ok&&!matched&&index<rule->identity_count;++index){
      status=srl_ext_scalar_identity(selected,text.data,&rule->identities[index],scope,work,&matched);
    }if(status==srl_ok&&!matched){status=srl_invalid;}
  }
  if(status==srl_ok&&rule->fixed_identity){bool matched=false;status=srl_ext_scalar_identity(selected,text.data,rule->fixed_identity,scope,work,&matched);if(status==srl_ok&&!matched){status=srl_invalid;}}
  if(status==srl_ok){srl_string_free(result);*result=text;if(selected_leaf){*selected_leaf=selected;}}else{srl_string_free(&text);}return status;
}
/* Apply full scalar restrictions in scope before the filtered document can publish native values. */
static inline srl_status srl_ext_namespace_value(const char* raw,unsigned kind,const srl_ext_scope* scope,
    size_t maximum_items,const srl_xml_namespace_list* enumeration,size_t enumeration_count,const srl_xml_scalar_rule* rule) {
  if(!kind){return srl_ok;}
  if(rule){srl_xml_scalar_work work={srl_xml_scalar_max_work,maximum_items};srl_string checked={0};
    srl_status status=srl_ext_scalar_selected(rule,raw,scope,&work,&checked,NULL);srl_string_free(&checked);if(status!=srl_ok){return status;}}
  return kind==4u ? srl_ok : srl_ext_namespace_tokens(raw,kind,scope,maximum_items,enumeration,enumeration_count);
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
/* Rebind private writer type aliases without changing retained QName or opaque lexical scopes. */
static inline srl_status srl_ext_replay_type(srl_ext_head* head,srl_ext_scope* scope) {
  for (size_t index=0; index<head->attributes.size; ++index) {
    srl_ext_attribute* attribute=&head->attributes.data[index];
    if (strcmp(attribute->uri,"http://www.w3.org/2001/XMLSchema-instance") || strcmp(attribute->local,"type")) { continue; }
    char* raw=srl_ext_copy(attribute->value); if (!raw) { return srl_allocation; }
    char* colon=strchr(raw,':');const char* local=colon ? colon+1 : raw;if (colon) { *colon=0; }
    const char* uri=srl_ext_namespace(&head->scope,colon ? raw : "");if (!uri) { uri=""; }
    const char* prefix=NULL;
    for (size_t binding=0; binding<scope->size; ++binding) { if (!strcmp(scope->data[binding].uri,uri)) { prefix=scope->data[binding].prefix;break; } }
    if (!*uri && !prefix) { const char* current=srl_ext_namespace(scope,"");if (current && *current) { free(raw);return srl_invalid; }prefix=""; }
    if (!prefix) {
      char candidate[sizeof("srl_type")+3*sizeof(size_t)];size_t ordinal=0;
      for (;;) { const int count=snprintf(candidate,sizeof(candidate),"srl_type%zu",ordinal);
        if (count<0 || (size_t)count>=sizeof(candidate)) { free(raw);return srl_limit; }
        if (!srl_ext_namespace(scope,candidate)) { break; }
        if (ordinal==SIZE_MAX) { free(raw);return srl_limit; }++ordinal;
      }
      if (!srl_ext_binding_add(scope,candidate,uri)) { free(raw);return srl_allocation; }
      prefix=scope->data[scope->size-1].prefix;
    }
    const size_t prefix_size=strlen(prefix),local_size=strlen(local);
    if (local_size>SIZE_MAX-2 || prefix_size>SIZE_MAX-local_size-2) { free(raw);return srl_limit; }
    char* value=(char*)malloc(prefix_size+local_size+2);if (!value) { free(raw);return srl_allocation; }
    if (prefix_size) { memcpy(value,prefix,prefix_size);value[prefix_size]=':';memcpy(value+prefix_size+1,local,local_size+1); }
    else { memcpy(value,local,local_size+1); }
    free(raw);free(attribute->value);attribute->value=value;
  }return srl_ok;
}
/* Charge every retained allocation before its logical payload is copied. */
static inline srl_status srl_ext_charge(srl_xml_extension_state* state, const srl_xml_extension_options* options, size_t size) {
  if (state->validation_only) { return srl_ok; }
  if (state->preserved_bytes > options->limits.max_preserved_bytes || size > options->limits.max_preserved_bytes-state->preserved_bytes) { return srl_limit; } state->preserved_bytes+=size; return srl_ok;
}
/* Bound transport framing before growing its contiguous byte buffer. */
static inline bool srl_ext_put(srl_ext_output* output, const char* value, size_t size) {
  if (output->writer.status != srl_ok) { return false; }
  if (output->writer.buffer.size > output->maximum || size > output->maximum-output->writer.buffer.size) { output->writer.status=srl_limit; return false; }
  srl_put(&output->writer,value,size); return output->writer.status==srl_ok;
}
/* Validate complete provider-split scalar text, adding only an uncharged private terminator. */
static inline srl_status srl_ext_namespace_finish(srl_ext_output* value, unsigned kind,
    const srl_ext_scope* scope, size_t maximum_items,
    const srl_xml_namespace_list* enumeration, size_t enumeration_count,const srl_xml_scalar_rule* rule) {
  if (!kind) { return srl_ok; }
  if (value->writer.buffer.size==SIZE_MAX) { return srl_limit; }
  if (!srl_reserve(&value->writer.buffer,1u)) { return srl_allocation; }
  value->writer.buffer.data[value->writer.buffer.size]=0;
  return srl_ext_namespace_value((const char*)value->writer.buffer.data,kind,scope,maximum_items,enumeration,enumeration_count,rule);
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
/* Evaluate generated wildcard predicates while preserving authored slots' foreign-only meaning. */
static inline bool srl_ext_namespace_match(const srl_xml_extension_contract* contract,const srl_xml_extension_slot* slot,const char* uri) {
  if ((!slot->namespace_match||slot->namespace_match==4) && (!*uri || !strcmp(uri,contract->core))) { return false; }
  if (slot->namespace_match==1) { return true; }
  bool listed=false;
  for (size_t index=0; index<slot->uri_count; ++index) { if (!strcmp(uri,slot->uris[index])) { listed=true; break; } }
  return slot->namespace_match==3||slot->namespace_match==4 ? !listed : listed;
}
/* Match one exact boundary after kind-aware known-field dispatch, including absent namespace predicates. */
static inline size_t srl_ext_slot(const srl_xml_extension_model* model, const char* uri, const char* local, bool attribute, size_t position) {
  const srl_xml_extension_contract* contract=model->contract; if (!contract || !strcmp(uri,"urn:serializer:xml:model")) { return SIZE_MAX; }
  bool known=false;
  for (size_t index=0; index<model->field_count; ++index) { if (attribute==model->fields[index].attribute && !strcmp(uri,model->fields[index].uri) && !strcmp(local,model->fields[index].local)) { known=true; break; } }
  for (size_t index=0; index<contract->slot_count; ++index) { const srl_xml_extension_slot* slot=&contract->slots[index]; if (slot->attribute!=attribute || (!attribute && slot->position!=SIZE_MAX && slot->position!=position)) { continue; }
    if (known && (attribute || !slot->namespace_match || slot->namespace_match==4)) { continue; }
    if (srl_ext_namespace_match(contract,slot,uri)) { return index; }
  } return SIZE_MAX;
}
/* Required generated wildcards take their fixed leading occurrence before the next native field. */
static inline bool srl_ext_required_wildcard(const srl_xml_extension_model* model,const char* uri,size_t position,size_t field,const size_t* counts) {
  if (!model->contract) { return false; }
  for (size_t index=0; index<model->contract->slot_count; ++index) {
    const srl_xml_extension_slot* slot=&model->contract->slots[index];
    if (!slot->attribute && slot->namespace_match && slot->position>=position && slot->position<=field &&
        counts[index]<slot->minimum && srl_ext_namespace_match(model->contract,slot,uri)) { return true; }
  }
  return false;
}
/* Charge preserve and skip roots globally before traversing their bounded payloads. */
static inline srl_status srl_ext_admit(srl_xml_extension_state* state, const srl_xml_extension_model* model, size_t* counts, size_t slot, const srl_xml_extension_options* options, bool* preserve) {
  if (!model->contract || slot>=model->contract->slot_count) { return srl_invalid; } const srl_xml_extension_slot* selected=&model->contract->slots[slot];
  if (!selected->policy) { return srl_invalid; } if (state->items>=options->limits.max_items || counts[slot]>=selected->maximum) { return srl_limit; }
  ++state->items; ++counts[slot]; *preserve=!state->validation_only && selected->policy==2; return srl_ok;
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
/* Record schema-origin defaults separately from occurrence namespace bindings; SIZE_MAX denotes text. */
static inline srl_status srl_ext_default_add(srl_xml_extension_state* state,const srl_xml_extension_model* model,
    const srl_buffer* path,const srl_ext_scope* scope,const srl_xml_extension_options* options,size_t field) {
  size_t index=0;srl_status status=srl_ext_owner_get(state,model,path,scope,options,&index);if(status!=srl_ok){return status;}
  status=srl_ext_charge(state,options,sizeof(field));if(status!=srl_ok){return status;}srl_ext_owner* owner=&state->owners[index];
  if(!srl_ext_grow((void**)&owner->defaults,&owner->default_capacity,owner->default_count+1,sizeof(field))){return srl_allocation;}
  owner->defaults[owner->default_count++]=field;return srl_ok;
}
/* Only an unchanged value originally supplied by the schema may be omitted to reapply that literal. */
static inline bool srl_ext_default_value(const srl_ext_owner* owner,size_t field,const srl_xml_scalar_rule* rule,const char* value) {
  if(!owner||!rule||!rule->schema_default||strcmp(value,rule->schema_default)){return false;}
  for(size_t index=0;index<owner->default_count;++index){if(owner->defaults[index]==field){return true;}}return false;
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
/* Replace an entry ordinal with a length-delimited canonical key, avoiding hashes and native layout changes. */
static inline srl_status srl_ext_map_path(srl_writer* path,const char* text,unsigned kind) {
  if (path->buffer.size<2*sizeof(size_t)) { return srl_invalid; }
  size_t size=strlen(text);bool negative=false;
  if (kind!=srl_ext_map_lexical_key) {
    while(size&&srl_xml_scalar_space((unsigned char)*text)){++text;--size;}
    while(size&&srl_xml_scalar_space((unsigned char)text[size-1])){--size;}
    if(kind==srl_ext_map_boolean_key){
      if((size==1&&*text=='1')||(size==4&&!memcmp(text,"true",4))){text="1";size=1;}
      else if((size==1&&*text=='0')||(size==5&&!memcmp(text,"false",5))){text="0";size=1;}
      else{return srl_invalid;}
    }else{
      if(size&&(*text=='+'||*text=='-')){negative=*text=='-';++text;--size;}
      if(!size){return srl_invalid;}
      for(size_t index=0;index<size;++index){if(text[index]<'0'||text[index]>'9'){return srl_invalid;}}
      while(size&&*text=='0'){++text;--size;}
      if(!size){text="0";size=1;negative=false;}
    }
  }
  const size_t marker=SIZE_MAX;
  memcpy(path->buffer.data+path->buffer.size-sizeof(marker),&marker,sizeof(marker));
  if(negative&&size==SIZE_MAX){return srl_limit;}const size_t key_size=size+(negative?1u:0u);
  srl_put(path,&key_size,sizeof(key_size));if(negative){srl_put(path,"-",1);}srl_put(path,text,size);return path->status;
}
/* Consume one bounded scalar map key and reproduce its original expanded element in known transport. */
static inline srl_status srl_ext_map_scalar(srl_xml_input* input,srl_ext_output* output,const srl_ext_scope* parent,const srl_xml_extension_field* field,srl_writer* path,unsigned kind) {
  srl_ext_head head;srl_status status=srl_ext_head_read(input,&head,parent);char* lexical=NULL;xmlChar* value=NULL;
  if(status==srl_ok){status=srl_ext_start(output,&head,&head.scope,&head.attributes,&lexical);}
  if(status==srl_ok){value=srl_xml_input_scalar(input,field->local,field->uri);if(!value){status=input->status==srl_ok?srl_invalid:input->status;}}
  if(status==srl_ok){status=srl_ext_map_path(path,(const char*)value,kind);}
  if(status==srl_ok&&!srl_ext_escape(output,(const char*)value,false)){status=output->writer.status;}
  if(status==srl_ok){status=srl_ext_end(output,lexical);}
  xmlFree(value);free(lexical);srl_ext_head_free(&head);return status;
}
/* Bind an attribute key before any child companion can be retained or replayed. */
static inline srl_status srl_ext_map_attribute(srl_writer* path,const srl_xml_extension_model* model,const srl_ext_attributes* attributes) {
  const srl_xml_extension_field* key=&model->fields[0];
  for(size_t index=0;index<attributes->size;++index){const srl_ext_attribute* value=&attributes->data[index];
    if(!strcmp(value->uri,key->uri)&&!strcmp(value->local,key->local)){return srl_ext_map_path(path,value->value,model->map_key_kind);}
  }
  return srl_invalid;
}
/* Reject strict absence before capture, and keep undeclared lax/skip traversal free of owning subtree copies. */
static inline srl_status srl_ext_declared(const srl_xml_extension_model* model,size_t slot,const char* uri,const char* local,const srl_limits* limits,srl_xml_operation* admission,bool* declared) {
  if (!model->contract || slot>=model->contract->slot_count || !declared) { return srl_invalid; }
  *declared=false;const unsigned process=model->contract->slots[slot].process_contents;
  if (!process) { return srl_ok; }
  srl_status status=srl_ok;
  if (model->contract->validate) { status=model->contract->validate(uri,local,NULL,NULL,NULL,0,limits,admission,declared); }
  return status!=srl_ok?status:!*declared&&process==2?srl_invalid:srl_ok;)cext"
R"cext(
}
/* This reader borrows a stack callback context; its caller releases only the independently allocated path. */
static inline void srl_ext_declaration_input_free(srl_xml_input* input) {
  srl_xml_input_free(input);
}
/* Validate against a pinned declaration while retaining the original operation's admission latch. */
static inline srl_status srl_ext_validate(const srl_xml_extension_model* model,size_t slot,const char* uri,const char* local,const char* value,const srl_ext_scope* scope,const uint8_t* bytes,size_t size,const srl_limits* limits,srl_xml_operation* admission) {
  if (!model->contract || slot>=model->contract->slot_count) { return srl_invalid; }
  const unsigned process=model->contract->slots[slot].process_contents;
  if (!process) { return srl_ok; }
  if (!model->contract->validate) { return process==1 ? srl_ok : srl_invalid; }
  bool found=false;
  if (value) { const srl_status status=model->contract->validate(uri,local,value,scope,NULL,0,limits,admission,&found);return status!=srl_ok ? status : !found&&process==2 ? srl_invalid : srl_ok; }
  srl_xml_input input;srl_xml_operation_reader reader={0};reader.operation=admission;
  srl_status status=srl_xml_operation_input_init(&input,bytes,size,limits,admission?&reader:NULL);
  srl_ext_head head={0};
  if (status==srl_ok && !srl_xml_input_space(&input)) { status=input.status==srl_ok?srl_invalid:input.status; }
  if (status==srl_ok) { status=srl_ext_head_read(&input,&head,NULL); }
  if (status==srl_ok) { status=model->contract->validate(head.uri,head.local,NULL,&head.scope,bytes,size,limits,admission,&found); }
  srl_ext_head_free(&head);srl_ext_declaration_input_free(&input);free(reader.path);
  return status!=srl_ok ? status : !found&&process==2 ? srl_invalid : srl_ok;
}
/* Borrow a provider reader's operation; ordinary XML decoding has no operation attached. */
static inline srl_xml_operation* srl_ext_admission(srl_xml_input* input) {
  srl_xml_operation_reader* context=srl_xml_operation_reader_context(input->reader);
  return context?context->operation:NULL;
}
/* Prepare a scalar attribute without changing its QName scope or retaining an owning native model. */
static inline srl_status srl_ext_declaration_attribute(const char* root_uri,const char* root_local,const char* uri,const char* local,const char* value,const srl_ext_scope* scope,size_t maximum,srl_buffer* result) {
  srl_ext_head head={0};head.uri=(char*)root_uri;head.local=(char*)root_local;head.prefix="";
  srl_ext_attribute borrowed={(char*)uri,(char*)local,(char*)value};
  srl_ext_attributes attributes={&borrowed,1,1};srl_ext_output output={0};output.maximum=maximum;char* lexical=NULL;
  srl_status status=srl_ext_start(&output,&head,scope,&attributes,&lexical);
  if(status==srl_ok){status=srl_ext_end(&output,lexical);}free(lexical);
  if(status!=srl_ok){srl_buffer_free(&output.writer.buffer);return status;}srl_buffer_free(result);*result=output.writer.buffer;return srl_ok;
}
/* Compiled legal derivations retain their complete DTOs and abstract-type boundary. */
typedef struct srl_xml_declaration_type { const char* uri; const char* local; bool abstract_type; } srl_xml_declaration_type;
/* Select expanded xsi:type before stripping transport markers; undeclared/abstract types never become lax fallback. */
static inline srl_status srl_ext_declaration_type(const uint8_t* bytes,size_t size,const srl_limits* limits,const srl_xml_declaration_type* types,size_t type_count,size_t* selected) {
  if (!types || !type_count || !selected) { return srl_invalid; }
  *selected=0;srl_xml_input input;srl_status status=srl_xml_input_init(&input,bytes,size,limits);srl_ext_head head={0};srl_string normalized={0};
  if (status==srl_ok && !srl_xml_input_space(&input)) { status=input.status==srl_ok?srl_invalid:input.status; }
  if (status==srl_ok) { status=srl_ext_head_read(&input,&head,NULL); }
  for (size_t index=0;status==srl_ok && index<head.attributes.size;++index) {
    const srl_ext_attribute* attribute=&head.attributes.data[index];
    if (strcmp(attribute->uri,"http://www.w3.org/2001/XMLSchema-instance") || strcmp(attribute->local,"type")) { continue; }
    status=srl_ext_namespace_tokens(attribute->value,1,&head.scope,limits->max_elements,NULL,0);
    if (status==srl_ok) { status=srl_xml_scalar_normalize(attribute->value,strlen(attribute->value),"collapse",&normalized); }
    if (status!=srl_ok) { break; }
    char* colon=strchr(normalized.data,':');const char* local=colon?colon+1:normalized.data;
    if (colon) { *colon=0; }
    const char* uri=srl_ext_namespace(&head.scope,colon?normalized.data:"");if (!uri) { uri=""; }
    bool matched=false;
    for (size_t choice=0;choice<type_count;++choice) {
      if (types[choice].uri && types[choice].local && !strcmp(uri,types[choice].uri) && !strcmp(local,types[choice].local)) { *selected=choice;matched=true;break; }
    }
    if (!matched) { status=srl_invalid; }
    break;
  }
  if (status==srl_ok && types[*selected].abstract_type) { status=srl_invalid; }
  srl_string_free(&normalized);srl_ext_head_free(&head);srl_xml_input_free(&input);return status;
}
/* A nil global still validates every attribute; only its content constraints are suspended. */
static inline srl_status srl_ext_declaration_element(const uint8_t* bytes,size_t size,const srl_limits* limits,bool nillable,bool fixed,const char* type_uri,const char* type_local,srl_buffer* result,bool* nil) {
  srl_xml_input input;srl_status status=srl_xml_input_init(&input,bytes,size,limits);srl_ext_head head={0};
  srl_ext_output output={0};output.maximum=limits->max_bytes;char* lexical=NULL;*nil=false;
  if(status==srl_ok&&!srl_xml_input_space(&input)){status=input.status==srl_ok?srl_invalid:input.status;}
  if(status==srl_ok){status=srl_ext_head_read(&input,&head,NULL);}
  for(size_t index=0;status==srl_ok&&index<head.attributes.size;){srl_ext_attribute* attribute=&head.attributes.data[index];
    if(strcmp(attribute->uri,"http://www.w3.org/2001/XMLSchema-instance")||(strcmp(attribute->local,"nil")&&strcmp(attribute->local,"type"))){++index;continue;}
    if(!strcmp(attribute->local,"type")){
      if(!type_uri||!type_local){status=srl_invalid;break;}
      const srl_xml_namespace_name expected={type_uri,type_local};const srl_xml_namespace_list allowed={&expected,1};
      status=srl_ext_namespace_tokens(attribute->value,1,&head.scope,limits->max_elements,&allowed,1);if(status!=srl_ok){break;}
    }else{
      const char* value=attribute->value;size_t length=strlen(value);while(length&&srl_xml_scalar_space((unsigned char)*value)){++value;--length;}while(length&&srl_xml_scalar_space((unsigned char)value[length-1])){--length;}
      const bool yes=(length==1&&*value=='1')||(length==4&&!memcmp(value,"true",4));
      const bool no=(length==1&&*value=='0')||(length==5&&!memcmp(value,"false",5));
      if(!nillable||(!yes&&!no)||(yes&&fixed)){status=srl_invalid;break;}*nil=yes;
    }
    free(attribute->uri);free(attribute->local);free(attribute->value);memmove(attribute,attribute+1,(head.attributes.size-index-1)*sizeof(*attribute));--head.attributes.size;
  }
  if(status==srl_ok){status=srl_ext_start(&output,&head,&head.scope,&head.attributes,&lexical);}
  if(status==srl_ok&&!srl_xml_input_next(&input)){status=input.status;}
  while(status==srl_ok&&input.kind!=XML_READER_TYPE_END_ELEMENT){
    if(input.kind==XML_READER_TYPE_ELEMENT){status=*nil?srl_invalid:srl_ext_foreign(&input,&output,&head.scope);}
    else if(input.kind==XML_READER_TYPE_NONE){status=srl_invalid;}
    else{const char* value=(const char*)xmlTextReaderConstValue(input.reader);if(*nil&&value&&*value){status=srl_invalid;break;}if(!srl_ext_escape(&output,value?value:"",false)){status=output.writer.status;}if(status==srl_ok&&!srl_xml_input_next(&input)){status=input.status;}}
  }
  if(status==srl_ok){status=srl_ext_end(&output,lexical);}if(status==srl_ok&&(!srl_xml_input_next(&input)||!srl_xml_input_finish(&input))){status=input.status==srl_ok?srl_invalid:input.status;}
  free(lexical);srl_ext_head_free(&head);srl_xml_input_free(&input);
  if(status!=srl_ok){srl_buffer_free(&output.writer.buffer);return status;}srl_buffer_free(result);*result=output.writer.buffer;return srl_ok;
}
/* Filter an ordinary typed owner while recording exact static-field/repeated-item positions. */
static inline srl_status srl_ext_filter_owner(srl_xml_input* input, srl_ext_output* output, const srl_xml_extension_model* model, srl_writer* path, srl_xml_extension_state* state, const srl_xml_extension_options* options, const srl_ext_scope* parent) {
  srl_ext_head head; srl_status status=srl_ext_head_read(input,&head,parent); char* lexical=NULL; size_t position=0;
  /* Only child slots impose static positions; the typed decoder checks mixed/all child grammar. */
  bool positioned=false,closed_children=false;
  if (model->contract) { for (size_t index=0; index<model->contract->slot_count; ++index) { positioned=positioned || !model->contract->slots[index].attribute; } }
  positioned=positioned && !model->flexible_children;
  srl_ext_output namespace_value={0};namespace_value.maximum=input->limits.max_string_bytes;bool namespace_owner=!state->validation_only && model->namespace_text!=0;
  const unsigned namespace_kind=srl_ext_nil(&head.attributes) ? 0 : model->namespace_text;
  const size_t original_owners=state->owner_count;
  const size_t slot_count=model->contract ? model->contract->slot_count : 0;
  size_t* counts=(size_t*)calloc(slot_count ? slot_count : 1,sizeof(size_t)); size_t* items=(size_t*)calloc(model->field_count ? model->field_count : 1,sizeof(size_t));
  srl_ext_attributes known={0}; if (!counts || !items) { status=srl_allocation; }
  if (status==srl_ok) { status=srl_ext_capabilities(model->contract,&head.scope,options); }
  for (size_t index=0; status==srl_ok && index<head.attributes.size; ++index) { srl_ext_attribute* attribute=&head.attributes.data[index]; bool recognized=!strcmp(attribute->uri,"urn:serializer:xml:model") || (!strcmp(attribute->uri,"http://www.w3.org/2001/XMLSchema-instance") && (!strcmp(attribute->local,"nil") || !strcmp(attribute->local,"type")));
    for (size_t field=0; !recognized && field<model->field_count; ++field) { recognized=model->fields[field].attribute && !strcmp(attribute->uri,model->fields[field].uri) && !strcmp(attribute->local,model->fields[field].local);
      if (recognized && model->fields[field].namespace_kind) { namespace_owner=!state->validation_only;status=srl_ext_namespace_value(attribute->value,model->fields[field].namespace_kind,&head.scope,input->limits.max_elements,model->fields[field].namespace_enumeration,model->fields[field].namespace_enumeration_count,model->fields[field].scalar_rule); }
    }
    if (recognized) { if (!srl_ext_grow((void**)&known.data,&known.capacity,known.size+1,sizeof(*known.data))) { status=srl_allocation; break; } known.data[known.size++]=*attribute; }
    else { const size_t slot=srl_ext_slot(model,attribute->uri,attribute->local,true,0); bool preserve=false; status=srl_ext_admit(state,model,counts,slot,options,&preserve);
      if (status==srl_ok) { status=srl_ext_validate(model,slot,attribute->uri,attribute->local,attribute->value,&head.scope,NULL,0,&input->limits,srl_ext_admission(input)); }
      if (status==srl_ok && preserve) { size_t owner=0; status=srl_ext_owner_get(state,model,&path->buffer,&head.scope,options,&owner);
        const size_t a=strlen(attribute->uri),b=strlen(attribute->local),c=strlen(attribute->value);
        if (status==srl_ok && (a>SIZE_MAX-48 || b>SIZE_MAX-48-a || c>SIZE_MAX-48-a-b)) { status=srl_limit; }
        if (status==srl_ok) { status=srl_ext_charge(state,options,48+a+b+c); }
        if (status==srl_ok) { status=srl_ext_attribute_copy(&state->owners[owner].attributes,attribute); }
      }
    }
  }
  bool map_key_ready=model->map_key_kind==0;
  for(size_t field=0;status==srl_ok&&field<model->field_count;++field){
    const srl_xml_extension_field* selected=&model->fields[field];
    if(!selected->attribute||!selected->scalar_rule||!selected->scalar_rule->schema_default){continue;}
    bool present=false;for(size_t index=0;index<head.attributes.size;++index){const srl_ext_attribute* value=&head.attributes.data[index];present=present||(!strcmp(value->uri,selected->uri)&&!strcmp(value->local,selected->local));}
    if(!present&&!state->validation_only){status=srl_ext_default_add(state,model,&path->buffer,&head.scope,options,field);}
  }
  if(status==srl_ok&&model->map_key_kind&&model->fields[0].attribute){status=srl_ext_map_attribute(path,model,&head.attributes);map_key_ready=status==srl_ok;}
  if (status==srl_ok && namespace_owner) { size_t owner=0;status=srl_ext_owner_get(state,model,&path->buffer,&head.scope,options,&owner); }
  if (status==srl_ok) { status=srl_ext_start(output,&head,&head.scope,&known,&lexical); } free(known.data);
  if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; }
  while (status==srl_ok && input->kind!=XML_READER_TYPE_END_ELEMENT) {
    if (input->kind==XML_READER_TYPE_ELEMENT) { const char* uri=(const char*)xmlTextReaderConstNamespaceUri(input->reader); if (!uri) { uri=""; } const char* local=(const char*)xmlTextReaderConstLocalName(input->reader); size_t selected=SIZE_MAX;
      for (size_t index=positioned ? position : 0; index<model->field_count; ++index) { const srl_xml_extension_field* field=&model->fields[index]; if (!field->attribute && !strcmp(uri,field->uri) && !strcmp(local,field->local) && srl_ext_typed_field(input,field)) { selected=index; break; } }
      if (selected!=SIZE_MAX && srl_ext_required_wildcard(model,uri,position,selected,counts)) { selected=SIZE_MAX; }
      if (selected!=SIZE_MAX) { const srl_xml_extension_field* field=&model->fields[selected]; const size_t item=items[selected]++; position=field->repeated ? selected : selected+1;
        if (closed_children) { status=srl_invalid;break; }
        if(model->map_key_kind&&selected==0&&!field->attribute){)cext"
R"cext(
          if(map_key_ready){status=srl_invalid;break;}
          status=srl_ext_map_scalar(input,output,&head.scope,field,path,model->map_key_kind);map_key_ready=status==srl_ok;continue;
        }
        if(!map_key_ready){status=srl_invalid;break;}
        if (field->child) { const size_t rollback=path->buffer.size; srl_put(path,&selected,sizeof(selected)); srl_put(path,&item,sizeof(item)); status=path->status;
          if (status==srl_ok) { status=srl_ext_filter_owner(input,output,field->child(),path,state,options,&head.scope); } path->buffer.size=rollback;
        } else { status=srl_ext_foreign(input,output,&head.scope); }
      } else { size_t slot=SIZE_MAX; for (size_t index=model->flexible_children ? model->field_count : position; index<=model->field_count; ++index) { if (index<model->field_count && model->fields[index].attribute) { continue; } slot=srl_ext_slot(model,uri,local,false,index); if (slot!=SIZE_MAX) { position=index; break; } }
        closed_children=model->flexible_children;
        bool preserve=false; status=srl_ext_admit(state,model,counts,slot,options,&preserve);
        bool validating=false;if(status==srl_ok){status=srl_ext_declared(model,slot,uri,local,&input->limits,srl_ext_admission(input),&validating);}
        if (status==srl_ok && (preserve||validating)) { size_t owner=0;
          if(preserve){status=srl_ext_owner_get(state,model,&path->buffer,&head.scope,options,&owner);} const size_t uri_size=strlen(uri); char* copied_uri=NULL; srl_ext_output captured={0};
          if (status==srl_ok&&preserve) { status=uri_size>SIZE_MAX-24 ? srl_limit : srl_ext_charge(state,options,24+uri_size); }
          if (status==srl_ok&&preserve) { copied_uri=srl_ext_copy(uri); if (!copied_uri) { status=srl_allocation; } }
          captured.maximum=preserve?options->limits.max_preserved_bytes-state->preserved_bytes:input->limits.max_bytes;
          if (status==srl_ok) { status=srl_ext_foreign(input,&captured,&head.scope); }
          if (status==srl_ok) { status=srl_ext_validate(model,slot,"","",NULL,NULL,captured.writer.buffer.data,captured.writer.buffer.size,&input->limits,srl_ext_admission(input)); }
          if (status==srl_ok&&preserve) { status=srl_ext_charge(state,options,captured.writer.buffer.size); }
          if (status==srl_ok&&preserve) { srl_ext_owner* target=&state->owners[owner]; if (!srl_ext_grow((void**)&target->children,&target->child_capacity,target->child_count+1,sizeof(*target->children))) { status=srl_allocation; }
            else { const srl_ext_child child={position,position<model->field_count ? items[position] : 0,copied_uri,captured.writer.buffer}; target->children[target->child_count++]=child; copied_uri=NULL; memset(&captured.writer.buffer,0,sizeof(captured.writer.buffer)); }
          } free(copied_uri); srl_buffer_free(&captured.writer.buffer);
        } else if (status==srl_ok) { status=srl_ext_foreign(input,NULL,&head.scope); }
      }
    } else if (input->kind==XML_READER_TYPE_NONE) { status=srl_invalid; }
    else { const char* text=(const char*)xmlTextReaderConstValue(input->reader);
      if (closed_children && model->mixed_text && text && *text) { status=srl_invalid;break; }
      if (model->namespace_text && !srl_ext_put(&namespace_value,text,strlen(text))) { status=namespace_value.writer.status; }
      if (status==srl_ok && !srl_ext_escape(output,text,false)) { status=output->writer.status; } if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; } }
  }
  if(status==srl_ok&&namespace_kind&&model->scalar_rule&&model->scalar_rule->schema_default&&!namespace_value.writer.buffer.size){
    if(!state->validation_only){status=srl_ext_default_add(state,model,&path->buffer,&head.scope,options,SIZE_MAX);}
  }else if(status==srl_ok){status=srl_ext_namespace_finish(&namespace_value,namespace_kind,&head.scope,input->limits.max_elements,model->namespace_enumeration,model->namespace_enumeration_count,model->scalar_rule);}
  if(status==srl_ok&&!map_key_ready){status=srl_invalid;}
  if (status==srl_ok && !state->validation_only && !model->map_key_kind && state->owner_count>original_owners) { size_t context_owner=0; status=srl_ext_owner_get(state,model,&path->buffer,&head.scope,options,&context_owner); }
  if (status==srl_ok) { status=srl_ext_minimum(model,counts); } if (status==srl_ok) { status=srl_ext_end(output,lexical); } if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; }
  srl_buffer_free(&namespace_value.writer.buffer);free(counts); free(items); free(lexical); srl_ext_head_free(&head); return status;
}
)cext"
R"cext(
/* Emit complete retained roots at the exact original field/item boundary. */
static inline srl_status srl_ext_replay_children(srl_ext_output* output, const srl_ext_owner* owner, size_t* cursor, size_t index, const size_t* items, const srl_xml_extension_model* model, size_t* counts, srl_xml_extension_state* admission, const srl_xml_extension_options* options) {
  if (!owner) { return srl_ok; }
  while (*cursor<owner->child_count) { const srl_ext_child* child=&owner->children[*cursor]; if (child->position>model->field_count) { return srl_invalid; } if (child->position>index) { break; }
    const size_t count=child->position<model->field_count ? items[child->position] : 0; if (child->position==index && child->item>count) { break; } if (child->item!=count) { return srl_invalid; }
    const size_t slot=srl_ext_slot(model,child->uri,"",false,child->position); bool preserve=false; srl_status status=srl_ext_admit(admission,model,counts,slot,options,&preserve); if (status!=srl_ok || !preserve) { return status==srl_ok ? srl_invalid : status; }
    status=srl_ext_validate(model,slot,"","",NULL,NULL,child->xml.data,child->xml.size,output->declaration_limits,output->declaration_operation);if(status!=srl_ok){return status;}
    if (!srl_ext_put(output,(const char*)child->xml.data,child->xml.size)) { return output->writer.status; } ++*cursor;
  } return srl_ok;
}
/* Replay borrowed sidecar payloads while traversing the generated codec's known XML stream. */
static inline srl_status srl_ext_replay_owner(srl_xml_input* input, srl_ext_output* output, const srl_xml_extension_model* model, srl_writer* path, const srl_xml_extension_state* state, bool* used, srl_xml_extension_state* admission, const srl_xml_extension_options* options, const srl_ext_scope* parent) {
  const srl_ext_owner* owner=NULL;
  if(!model->map_key_kind){for (size_t index=0; index<state->owner_count; ++index) { const srl_buffer* stored=&state->owners[index].path; if (stored->size==path->buffer.size && (!stored->size || !memcmp(stored->data,path->buffer.data,stored->size))) { owner=&state->owners[index]; used[index]=true; break; } }}
  if (owner && strcmp(owner->identity,model->identity)) { return srl_invalid; }
  srl_ext_head head; srl_status status=srl_ext_head_read(input,&head,parent); char* lexical=NULL; size_t cursor=0;
  const unsigned namespace_kind=srl_ext_nil(&head.attributes) ? 0 : model->namespace_text;
  bool map_key_ready=model->map_key_kind==0;
  if(status==srl_ok&&model->map_key_kind&&model->fields[0].attribute){status=srl_ext_map_attribute(path,model,&head.attributes);map_key_ready=status==srl_ok;}
  srl_ext_output namespace_value={0};namespace_value.maximum=input->limits.max_string_bytes;
  srl_ext_scope overlay={0},type_scope={0}; const srl_ext_scope* scope=&head.scope;
  // A retained owner scope is complete. Exclude canonical known-writer aliases that were absent from original opaque XML.
  if (owner) { overlay=owner->scope;overlay.parent=NULL;scope=&overlay; }
  bool typed=false;for (size_t index=0;index<head.attributes.size;++index) {
    typed=typed || (!strcmp(head.attributes.data[index].uri,"http://www.w3.org/2001/XMLSchema-instance") && !strcmp(head.attributes.data[index].local,"type"));
  }
  if (status==srl_ok && typed) {
    if (!srl_ext_scope_copy(&type_scope,scope)) { status=srl_allocation; }
    else { scope=&type_scope;status=srl_ext_replay_type(&head,&type_scope); }
  }
  const size_t slot_count=model->contract ? model->contract->slot_count : 0;
  size_t* counts=(size_t*)calloc(slot_count ? slot_count : 1,sizeof(size_t)); size_t* items=(size_t*)calloc(model->field_count ? model->field_count : 1,sizeof(size_t)); srl_ext_attributes attributes={0};
  if (!counts || !items) { status=srl_allocation; } if (status==srl_ok) { status=srl_ext_capabilities(model->contract,scope,options); }
  if (status==srl_ok) { const size_t extra=owner ? owner->attributes.size : 0; if (extra>SIZE_MAX-head.attributes.size || !srl_ext_grow((void**)&attributes.data,&attributes.capacity,head.attributes.size+extra,sizeof(*attributes.data))) { status=srl_allocation; }
    else { for (size_t index=0; index<head.attributes.size; ++index) {
      const srl_ext_attribute* value=&head.attributes.data[index];bool omitted=false;
      for(size_t field=0;field<model->field_count;++field){const srl_xml_extension_field* selected=&model->fields[field];
        if(selected->attribute&&!strcmp(value->uri,selected->uri)&&!strcmp(value->local,selected->local)&&srl_ext_default_value(owner,field,selected->scalar_rule,value->value)){omitted=true;break;}}
      if(!omitted){attributes.data[attributes.size++]=*value;}
    } }
  }
  if (owner) { for (size_t index=0; status==srl_ok && index<owner->attributes.size; ++index) { const srl_ext_attribute* value=&owner->attributes.data[index]; const size_t slot=srl_ext_slot(model,value->uri,value->local,true,0); bool preserve=false;
    status=srl_ext_admit(admission,model,counts,slot,options,&preserve); if (status==srl_ok && !preserve) { status=srl_invalid; }
    if(status==srl_ok){status=srl_ext_validate(model,slot,value->uri,value->local,value->value,scope,NULL,0,output->declaration_limits,output->declaration_operation);}
    if (status==srl_ok) { attributes.data[attributes.size++]=*value; }
  }}
  for (size_t index=0;status==srl_ok && index<attributes.size;++index) {
    const srl_ext_attribute* attribute=&attributes.data[index];
    for (size_t field=0;status==srl_ok && field<model->field_count;++field) {
      const srl_xml_extension_field* selected=&model->fields[field];
      if (selected->attribute && selected->namespace_kind && !strcmp(attribute->uri,selected->uri) && !strcmp(attribute->local,selected->local)) {
        status=srl_ext_namespace_value(attribute->value,selected->namespace_kind,scope,input->limits.max_elements,selected->namespace_enumeration,selected->namespace_enumeration_count,selected->scalar_rule);
      }
    }
  }
  if (status==srl_ok) { status=srl_ext_start(output,&head,scope,&attributes,&lexical); } free(attributes.data);
  if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; }
  while (status==srl_ok && input->kind!=XML_READER_TYPE_END_ELEMENT) {
    if (input->kind==XML_READER_TYPE_ELEMENT) { const char* uri=(const char*)xmlTextReaderConstNamespaceUri(input->reader); if (!uri) { uri=""; } const char* local=(const char*)xmlTextReaderConstLocalName(input->reader); size_t selected=SIZE_MAX;
      for (size_t index=0; index<model->field_count; ++index) { const srl_xml_extension_field* field=&model->fields[index]; if (!field->attribute && !strcmp(uri,field->uri) && !strcmp(local,field->local) && srl_ext_typed_field(input,field)) { selected=index; break; } }
      if (selected==SIZE_MAX) { status=srl_invalid; break; }
      status=srl_ext_replay_children(output,owner,&cursor,selected,items,model,counts,admission,options);
      const srl_xml_extension_field* field=&model->fields[selected]; const size_t item=items[selected]++;
      if(status==srl_ok&&model->map_key_kind&&selected==0&&!field->attribute){
        if(map_key_ready){status=srl_invalid;break;}
        status=srl_ext_map_scalar(input,output,scope,field,path,model->map_key_kind);map_key_ready=status==srl_ok;continue;
      }
      if(status==srl_ok&&!map_key_ready){status=srl_invalid;break;}
      if (status==srl_ok && field->child) { const size_t rollback=path->buffer.size; srl_put(path,&selected,sizeof(selected)); srl_put(path,&item,sizeof(item)); status=path->status;
        if (status==srl_ok) { status=srl_ext_replay_owner(input,output,field->child(),path,state,used,admission,options,scope); } path->buffer.size=rollback;
      } else if (status==srl_ok) { status=srl_ext_foreign(input,output,scope); }
    } else if (input->kind==XML_READER_TYPE_NONE) { status=srl_invalid; }
    else { const char* text=(const char*)xmlTextReaderConstValue(input->reader);
      if (model->namespace_text && !srl_ext_put(&namespace_value,text,strlen(text))) { status=namespace_value.writer.status; }
      if (status==srl_ok && (!model->scalar_rule||!model->scalar_rule->schema_default) && !srl_ext_escape(output,text,false)) { status=output->writer.status; } if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; } }
  }
  if(status==srl_ok&&model->scalar_rule&&model->scalar_rule->schema_default){
    if(namespace_value.writer.buffer.size==SIZE_MAX||!srl_reserve(&namespace_value.writer.buffer,1u)){status=srl_allocation;}
    else{namespace_value.writer.buffer.data[namespace_value.writer.buffer.size]=0;const char* text=(const char*)namespace_value.writer.buffer.data;
      if(!srl_ext_default_value(owner,SIZE_MAX,model->scalar_rule,text)){
        status=srl_ext_namespace_finish(&namespace_value,namespace_kind,scope,input->limits.max_elements,model->namespace_enumeration,model->namespace_enumeration_count,model->scalar_rule);
        if(status==srl_ok&&!srl_ext_escape(output,text,false)){status=output->writer.status;}
      }
    }
  }else if(status==srl_ok){status=srl_ext_namespace_finish(&namespace_value,namespace_kind,scope,input->limits.max_elements,model->namespace_enumeration,model->namespace_enumeration_count,model->scalar_rule);}
  if(status==srl_ok&&!map_key_ready){status=srl_invalid;}
  if (status==srl_ok) { status=srl_ext_replay_children(output,owner,&cursor,model->field_count,items,model,counts,admission,options); }
  if (status==srl_ok && owner && cursor!=owner->child_count) { status=srl_invalid; }
  if (status==srl_ok) { status=srl_ext_minimum(model,counts); } if (status==srl_ok) { status=srl_ext_end(output,lexical); } if (status==srl_ok && !srl_xml_input_next(input)) { status=input->status; }
  srl_buffer_free(&namespace_value.writer.buffer);free(counts); free(items); free(lexical);srl_ext_scope_free(&type_scope); srl_ext_head_free(&head); return status;
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
static inline srl_status srl_xml_extension_filter_mode(const uint8_t* bytes, size_t size, const srl_limits* limits, const srl_xml_extension_options* selected_options, const srl_xml_extension_model* model, srl_buffer* known, srl_xml_extension_state** result, bool validation_only) {
  if (!limits || !model || !known || !result || (size && !bytes)) { return srl_invalid; }
  const srl_xml_extension_options defaults=srl_xml_default_extension_options(); const srl_xml_extension_options* options=selected_options ? selected_options : &defaults;
  const srl_status options_status=srl_ext_options(options); if (options_status!=srl_ok) { return options_status; }
  srl_limits framing; const srl_status framed=srl_xml_extension_output_limits(limits,&framing); if (framed!=srl_ok) { return framed; }
  srl_xml_input input; srl_status status=srl_xml_input_init(&input,bytes,size,limits); srl_ext_output output={0}; output.maximum=framing.max_bytes;
  if (status==srl_ok && !srl_xml_input_space(&input)) { status=input.status==srl_ok ? srl_invalid : input.status; }
  srl_xml_extension_state* state=(srl_xml_extension_state*)calloc(1,sizeof(*state)); srl_writer path={0}; if (!state) { status=srl_allocation; } else { state->validation_only=validation_only; }
  if (status==srl_ok) { status=srl_ext_filter_owner(&input,&output,model,&path,state,options,NULL); }
  if (status==srl_ok && !srl_xml_input_finish(&input)) { status=input.status==srl_ok ? srl_invalid : input.status; }
  srl_xml_input_free(&input); srl_buffer_free(&path.buffer);
  if (status!=srl_ok) { srl_buffer_free(&output.writer.buffer); srl_xml_extension_state_free(state); return status; }
  srl_buffer_free(known); *known=output.writer.buffer; srl_xml_extension_state_free(*result); *result=state; return srl_ok;
}
/* Preserve ordinary document semantics through the original entry point. */
static inline srl_status srl_xml_extension_filter(const uint8_t* bytes,size_t size,const srl_limits* limits,const srl_xml_extension_options* options,const srl_xml_extension_model* model,srl_buffer* known,srl_xml_extension_state** result) {
  return srl_xml_extension_filter_mode(bytes,size,limits,options,model,known,result,false);
}
/* Discard all private DTO presentation while validating nested wildcard declarations on the same operation. */
static inline srl_status srl_xml_extension_filter_validation(const uint8_t* bytes,size_t size,const srl_limits* limits,const srl_xml_extension_model* model,srl_buffer* known,srl_xml_extension_state** result) {
  return srl_xml_extension_filter_mode(bytes,size,limits,NULL,model,known,result,true);
}
/* Replay immutable borrowed sidecar storage, then re-admit every final byte/event before publication. */
static inline srl_status srl_xml_extension_replay(const uint8_t* bytes, size_t size, const srl_limits* limits, const srl_xml_extension_options* selected_options, const srl_xml_extension_model* model, const srl_xml_extension_state* selected_state, srl_buffer* result) {)cext"
R"cext(
  if (!limits || !model || !result || (size && !bytes)) { return srl_invalid; }
  const srl_xml_extension_options defaults=srl_xml_default_extension_options(); const srl_xml_extension_options* options=selected_options ? selected_options : &defaults;
  const srl_status options_status=srl_ext_options(options); if (options_status!=srl_ok) { return options_status; }
  const srl_xml_extension_state empty={0}; const srl_xml_extension_state* state=selected_state ? selected_state : &empty;
  if (state->preserved_bytes>options->limits.max_preserved_bytes) { return srl_limit; }
  bool* used=(bool*)calloc(state->owner_count ? state->owner_count : 1,sizeof(bool)); if (!used) { return srl_allocation; }
  srl_limits framing; srl_status status=srl_xml_extension_output_limits(limits,&framing); if (status!=srl_ok || size>framing.max_bytes) { free(used); return status==srl_ok ? srl_limit : status; }
  status=srl_xml_extension_transport_limits(size,limits,&framing); if (status!=srl_ok) { free(used); return status; }
  srl_xml_input input; status=srl_xml_input_init(&input,bytes,size,&framing); srl_ext_output output={0}; output.maximum=limits->max_bytes; srl_writer path={0}; srl_xml_extension_state admission={0};
  output.declaration_limits=limits;output.declaration_operation=srl_ext_admission(&input);
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
