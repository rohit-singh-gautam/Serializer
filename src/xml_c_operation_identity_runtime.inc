// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"copidentity(/* Separate active state carries explicit hooks without extending any existing identity aggregate. */
typedef struct srl_xml_operation_identity_state {
  const srl_xml_identity_scope* scope;srl_xml_identity_limits limits;
  srl_xml_identity_owned_name* path;size_t depth,path_capacity;
  srl_xml_identity_candidate* active;size_t active_count,active_capacity;
  srl_xml_identity_table* tables;size_t selected,tuples,bytes,work,owner_depth;bool complete;

  srl_xml_operation* operation;const srl_xml_identity_diagnostic_constraint* diagnostics;
} srl_xml_operation_identity_state;

/* Charge lexical and index work before scans, allocations or comparisons. */
static inline srl_status srl_xml_operation_identity_spend(srl_xml_operation_identity_state* state,size_t count) {
  if(!srl_xml_operation_charge(state->operation,srl_xml_operation_work,count)){return srl_invalid;}
  if(state->work>state->limits.max_work_units||count>state->limits.max_work_units-state->work){return srl_xml_operation_fail(state->operation,srl_limit);}state->work+=count;return srl_ok;
}

/* Grow an operation-local typed vector geometrically, checking multiplication and overflow first. */
static inline srl_status srl_xml_operation_identity_grow(srl_xml_operation* operation,void** data,size_t* capacity,size_t count,size_t width) {
  if(count<=*capacity){return srl_ok;}if(count>SIZE_MAX/width){return srl_xml_operation_fail(operation,srl_limit);}size_t next=*capacity?*capacity:4u;
  while(next<count){if(next>SIZE_MAX/2u){next=count;break;}next*=2u;}if(next>SIZE_MAX/width){next=count;}
  if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,(next-*capacity)*width)){return srl_invalid;}
  void* allocation=realloc(*data,next*width);if(!allocation){return srl_allocation;}*data=allocation;*capacity=next;return srl_ok;
}

/* Release one incomplete candidate, including every active scalar capture. */
static inline void srl_xml_operation_identity_candidate_free(srl_xml_identity_candidate* value,size_t fields) {
  if(value->values){for(size_t index=0;index<fields;++index){srl_string_free(&value->values[index]);}}
  if(value->captures){for(size_t index=0;index<fields;++index){srl_buffer_free(&value->captures[index].text);}}
  free(value->values);free(value->seen);free(value->captures);memset(value,0,sizeof(*value));
}

/* Release a private pass after success or any failure; no state is ever published for reuse. */
static inline void srl_xml_operation_identity_free(srl_xml_operation_identity_state* state) {
  for(size_t index=0;index<state->depth;++index){srl_string_free(&state->path[index].uri);srl_string_free(&state->path[index].local);}
  for(size_t index=0;index<state->active_count;++index){srl_xml_operation_identity_candidate_free(&state->active[index],state->scope->constraints[state->active[index].constraint].count);}
  if(state->tables){for(size_t index=0;index<state->scope->count;++index){srl_xml_identity_table* table=&state->tables[index];for(size_t item=0;item<table->count;++item){srl_string_free(&table->values[item]);}free(table->values);free(table->slots);}}
  free(state->path);free(state->active);free(state->tables);memset(state,0,sizeof(*state));
}

/* Admit compiler-owned constraints before allocating the table inventory. */
static inline srl_status srl_xml_operation_identity_init(srl_xml_operation_identity_state* state,const srl_xml_identity_scope* scope,const srl_xml_identity_limits* limits,size_t owner_depth,srl_xml_operation* operation,const srl_xml_identity_diagnostic_constraint* diagnostics) {
  memset(state,0,sizeof(*state));state->scope=scope;state->limits=*limits;state->owner_depth=owner_depth;state->operation=operation;state->diagnostics=diagnostics;
  if(scope->count>limits->max_constraints||scope->count>SIZE_MAX/sizeof(state->tables[0])){return srl_xml_operation_fail(state->operation,srl_limit);}
  for(size_t index=0;index<scope->count;++index){const srl_xml_identity_constraint* item=&scope->constraints[index];
    if(!item->count||item->count>limits->max_fields){return srl_xml_operation_fail(state->operation,srl_limit);}if(item->kind==2u&&(item->refer>=scope->count||scope->constraints[item->refer].kind==2u)){return srl_invalid;}}
  if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,scope->count*sizeof(state->tables[0]))){return srl_invalid;}
  state->tables=(srl_xml_identity_table*)calloc(scope->count,sizeof(state->tables[0]));return state->tables||!scope->count?srl_ok:srl_allocation;
}

/* Match prepared expanded paths against active ancestors without copying suffix vectors. */
static inline srl_status srl_xml_operation_identity_matches(srl_xml_operation_identity_state* state,size_t first,const srl_xml_identity_path* path,bool* result) {
  const size_t count=state->depth-first;*result=false;srl_status status=srl_xml_operation_identity_spend(state,count+path->count+1u);if(status!=srl_ok){return status;}
  if(path->descendants?count<path->count:count!=path->count){return srl_ok;}const size_t offset=path->descendants?state->depth-path->count:first;
  for(size_t index=0;index<path->count;++index){const srl_xml_identity_owned_name* actual=&state->path[offset+index];const srl_xml_identity_name* wanted=&path->steps[index];
    const size_t raw=actual->uri.size+actual->local.size+2u,expected=strlen(wanted->uri)+strlen(wanted->local)+2u;
    if((status=srl_xml_operation_identity_spend(state,(raw<expected?raw:expected)+1u))!=srl_ok){return status;}
    if(!srl_string_equal(&actual->uri,wanted->uri)||!srl_string_equal(&actual->local,wanted->local)){return srl_ok;}}
  *result=true;return srl_ok;
}

/* Append private tuple bytes with overflow/allocation checks; XML escaping is deliberately unrelated. */
static inline srl_status srl_xml_operation_identity_put(srl_xml_operation* operation,srl_buffer* target,const void* data,size_t size) {
  if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,size)||!srl_xml_operation_charge(operation,srl_xml_operation_work,size)){return srl_invalid;}if(!srl_reserve(target,size)){return srl_allocation;}if(size){memcpy(target->data+target->size,data,size);}target->size+=size;return srl_ok;
}

/* Canonicalize exact scalar values independently of native widths, sign aliases and prefix spellings. */
static inline srl_status srl_xml_operation_identity_key(srl_xml_operation_identity_state* state,const char* raw,size_t size,
    const srl_xml_identity_field* field,xmlTextReaderPtr reader,srl_string* result) {
  if(size>state->limits.max_tuple_bytes){return srl_xml_operation_fail(state->operation,srl_limit);}srl_status status=srl_xml_operation_identity_spend(state,size+1u);if(status!=srl_ok){return status;}
  const char* whitespace=strcmp(field->kind,"string")==0?"preserve":strcmp(field->kind,"normalizedString")==0?"replace":"collapse";
  const srl_xml_scalar_rule rule={field->kind,whitespace,{"","","",""},SIZE_MAX,SIZE_MAX,SIZE_MAX,
    NULL,0,false,NULL,0,"",NULL,0,NULL,false,NULL,0,NULL,NULL,NULL};
  srl_string text={0};status=srl_xml_operation_scalar_check(state->operation,&rule,raw,size,state->limits.max_tuple_bytes,&text);if(status!=srl_ok){return status;}srl_buffer key={0};
  if(strcmp(field->kind,"QName")==0){srl_string uri={0},local={0};status=srl_xml_identity_qname(reader,text.data,&uri,&local);
    if(status==srl_ok){status=srl_xml_operation_identity_put(state->operation,&key,"q",1u);}if(status==srl_ok){status=srl_xml_operation_identity_put(state->operation,&key,uri.data,uri.size);}if(status==srl_ok){status=srl_xml_operation_identity_put(state->operation,&key,"",1u);}if(status==srl_ok){status=srl_xml_operation_identity_put(state->operation,&key,local.data,local.size);}srl_string_free(&uri);srl_string_free(&local);
  }else if(strcmp(field->kind,"boolean")==0){bool value=false;if(!srl_xml_boolean(text.data,&value)){status=srl_invalid;}else{status=srl_xml_operation_identity_put(state->operation,&key,value?"btrue":"bfalse",value?5u:6u);}}
  else if(srl_xml_scalar_integer(field->kind)||strcmp(field->kind,"decimal")==0){const srl_xml_scalar_decimal value=srl_xml_scalar_decimal_parts(text.data);status=srl_xml_operation_identity_put(state->operation,&key,"d",1u);
    if(status==srl_ok&&value.negative){status=srl_xml_operation_identity_put(state->operation,&key,"-",1u);}if(status==srl_ok){status=srl_xml_operation_identity_put(state->operation,&key,value.whole_size?value.whole:"0",value.whole_size?value.whole_size:1u);}
    if(status==srl_ok&&value.fraction_size){status=srl_xml_operation_identity_put(state->operation,&key,".",1u);if(status==srl_ok){status=srl_xml_operation_identity_put(state->operation,&key,value.fraction,value.fraction_size);}}
  }else if(strcmp(field->kind,"float")==0||strcmp(field->kind,"double")==0){status=srl_invalid;}
  else{status=srl_xml_operation_identity_put(state->operation,&key,strcmp(field->kind,"anyURI")==0?"u":"s",1u);if(status==srl_ok){status=srl_xml_operation_identity_put(state->operation,&key,text.data,text.size);}}
  srl_string_free(&text);if(status==srl_ok){status=srl_xml_operation_string_set(state->operation,result,(const char*)key.data,key.size);}srl_buffer_free(&key);return status;
}

/* Retain one complete canonical field only after candidate tuple-byte admission. */
static inline srl_status srl_xml_operation_identity_store(srl_xml_operation_identity_state* state,srl_xml_identity_candidate* candidate,size_t index,srl_string* value) {
  const size_t overhead=value->size&&value->data[0]=='q'?2u:1u;const size_t bytes=value->size-overhead;
  if(candidate->bytes>state->limits.max_tuple_bytes||bytes>state->limits.max_tuple_bytes-candidate->bytes){return srl_xml_operation_fail(state->operation,srl_limit);}
  candidate->bytes+=bytes;candidate->values[index]=*value;memset(value,0,sizeof(*value));return srl_ok;
}

/* Hash full length-framed tuples after charging their bytes; collisions still require exact byte equality. */
static inline srl_status srl_xml_operation_identity_hash(srl_xml_operation_identity_state* state,const srl_string* value,size_t* hash) {
  srl_status status=srl_xml_operation_identity_spend(state,value->size+1u);if(status!=srl_ok){return status;}
  uint64_t result=UINT64_C(14695981039346656037);for(size_t index=0;index<value->size;++index){result^=(uint8_t)value->data[index];result*=UINT64_C(1099511628211);}*hash=(size_t)result;return srl_ok;
}

/* Rebuild power-of-two open-addressed indexes geometrically, preserving owned tuple storage. */
static inline srl_status srl_xml_operation_identity_rehash(srl_xml_operation_identity_state* state,srl_xml_identity_table* table,size_t count) {
  if(count>SIZE_MAX/sizeof(size_t)){return srl_xml_operation_fail(state->operation,srl_limit);}if(!srl_xml_operation_charge(state->operation,srl_xml_operation_allocation,count*sizeof(size_t))){return srl_invalid;}size_t* slots=(size_t*)calloc(count,sizeof(size_t));if(!slots){return srl_allocation;}
  srl_status status=srl_ok;for(size_t index=0;index<table->count;++index){size_t hash=0;status=srl_xml_operation_identity_hash(state,&table->values[index],&hash);if(status!=srl_ok){break;}
    size_t slot=hash&(count-1u);while(slots[slot]){if((status=srl_xml_operation_identity_spend(state,1u))!=srl_ok){break;}slot=(slot+1u)&(count-1u);}if(status!=srl_ok){break;}slots[slot]=index+1u;}
  if(status==srl_ok){free(table->slots);table->slots=slots;table->slot_count=count;}else{free(slots);}return status;
}

/* Find exact canonical equality without last-wins replacement or uncharged collision work. */
static inline srl_status srl_xml_operation_identity_find(srl_xml_operation_identity_state* state,const srl_xml_identity_table* table,const srl_string* value,size_t* slot,bool* present) {
  size_t hash=0;srl_status status=srl_xml_operation_identity_hash(state,value,&hash);if(status!=srl_ok){return status;}*present=false;*slot=0;if(!table->slot_count){return srl_ok;}
  *slot=hash&(table->slot_count-1u);while(table->slots[*slot]){const srl_string* expected=&table->values[table->slots[*slot]-1u];
    const size_t bytes=value->size<expected->size?value->size:expected->size;if((status=srl_xml_operation_identity_spend(state,bytes+1u))!=srl_ok){return status;}
    if(value->size==expected->size&&(!bytes||memcmp(value->data,expected->data,bytes)==0)){*present=true;return srl_ok;}*slot=(*slot+1u)&(table->slot_count-1u);}
  return srl_ok;
}

/* Index complete tuples only after aggregate byte/count admission; duplicate keyrefs retain one lookup entry. */
static inline srl_status srl_xml_operation_identity_insert(srl_xml_operation_identity_state* state,size_t constraint,srl_string* tuple) {
  srl_xml_identity_table* table=&state->tables[constraint];srl_status status=srl_ok;
  if(!table->slot_count||table->count>=table->slot_count/2u){if(table->slot_count>SIZE_MAX/2u){return srl_xml_operation_fail(state->operation,srl_limit);}status=srl_xml_operation_identity_rehash(state,table,table->slot_count?table->slot_count*2u:8u);if(status!=srl_ok){return status;}}
  size_t slot=0;bool present=false;status=srl_xml_operation_identity_find(state,table,tuple,&slot,&present);if(status!=srl_ok){return status;}
  if(present){return state->scope->constraints[constraint].kind==2u?srl_ok:srl_invalid;}
  void* allocation=table->values;status=srl_xml_operation_identity_grow(state->operation,&allocation,&table->capacity,table->count+1u,sizeof(table->values[0]));table->values=(srl_string*)allocation;if(status!=srl_ok){return status;}
  table->values[table->count]=*tuple;memset(tuple,0,sizeof(*tuple));table->slots[slot]=++table->count;return srl_ok;
})copidentity"
R"copidentity(

/* Observe original expanded names and attributes before typed callbacks consume their lexical metadata. */
static inline srl_status srl_xml_operation_identity_start(srl_xml_operation_identity_state* state,const srl_xml_identity_name* name,
    const srl_xml_identity_attribute* attributes,size_t attribute_count,xmlTextReaderPtr reader) {
  if(state->complete){return srl_invalid;}srl_status status=srl_xml_operation_identity_spend(state,strlen(name->uri)+strlen(name->local)+3u);if(status!=srl_ok){return status;}
  if(state->depth>=state->limits.max_depth){return srl_xml_operation_fail(state->operation,srl_limit);}
  for(size_t candidate=0;candidate<state->active_count;++candidate){const srl_xml_identity_candidate* item=&state->active[candidate];for(size_t field=0;field<state->scope->constraints[item->constraint].count;++field){if(item->captures[field].active){return srl_invalid;}}}
  void* allocation=state->path;status=srl_xml_operation_identity_grow(state->operation,&allocation,&state->path_capacity,state->depth+1u,sizeof(state->path[0]));state->path=(srl_xml_identity_owned_name*)allocation;if(status!=srl_ok){return status;}
  srl_xml_identity_owned_name owned={0};status=srl_xml_operation_string_set(state->operation,&owned.uri,name->uri,strlen(name->uri));if(status==srl_ok){status=srl_xml_operation_string_set(state->operation,&owned.local,name->local,strlen(name->local));}if(status!=srl_ok){srl_string_free(&owned.uri);srl_string_free(&owned.local);return status;}state->path[state->depth++]=owned;
  for(size_t index=0;index<state->scope->count;++index){const srl_xml_identity_constraint* constraint=&state->scope->constraints[index];bool matches=false;
    status=srl_xml_operation_identity_matches(state,1u,&constraint->selector,&matches);if(status!=srl_ok){return status;}if(!matches){continue;}
    if(state->selected>=state->limits.max_selected_nodes){return srl_xml_operation_fail(state->operation,srl_limit);}++state->selected;
    allocation=state->active;status=srl_xml_operation_identity_grow(state->operation,&allocation,&state->active_capacity,state->active_count+1u,sizeof(state->active[0]));state->active=(srl_xml_identity_candidate*)allocation;if(status!=srl_ok){return status;}
    srl_xml_identity_candidate candidate={0};candidate.constraint=index;candidate.depth=state->depth;
    if(constraint->count>SIZE_MAX/sizeof(candidate.values[0])||constraint->count>SIZE_MAX/sizeof(candidate.captures[0])){return srl_xml_operation_fail(state->operation,srl_limit);}
    if(!srl_xml_operation_charge(state->operation,srl_xml_operation_allocation,constraint->count*sizeof(candidate.values[0]))||!srl_xml_operation_charge(state->operation,srl_xml_operation_allocation,constraint->count*sizeof(candidate.seen[0]))||!srl_xml_operation_charge(state->operation,srl_xml_operation_allocation,constraint->count*sizeof(candidate.captures[0]))){return srl_invalid;}candidate.values=(srl_string*)calloc(constraint->count,sizeof(candidate.values[0]));candidate.seen=(bool*)calloc(constraint->count,sizeof(candidate.seen[0]));candidate.captures=(srl_xml_identity_capture*)calloc(constraint->count,sizeof(candidate.captures[0]));
    if(!candidate.values||!candidate.seen||!candidate.captures){srl_xml_operation_identity_candidate_free(&candidate,constraint->count);return srl_allocation;}state->active[state->active_count++]=candidate;
  }
  for(size_t candidate_index=0;candidate_index<state->active_count;++candidate_index){srl_xml_identity_candidate* candidate=&state->active[candidate_index];const srl_xml_identity_constraint* constraint=&state->scope->constraints[candidate->constraint];if(state->diagnostics){srl_xml_operation_select(state->operation,state->diagnostics[candidate->constraint].count?state->diagnostics[candidate->constraint].fields[0]:state->diagnostics[candidate->constraint].name);}
    for(size_t field_index=0;field_index<constraint->count;++field_index){const srl_xml_identity_field* field=&constraint->fields[field_index];if(state->diagnostics&&field_index<state->diagnostics[candidate->constraint].count){srl_xml_operation_select(state->operation,state->diagnostics[candidate->constraint].fields[field_index]);}bool matches=false;status=srl_xml_operation_identity_matches(state,candidate->depth,&field->path,&matches);if(status!=srl_ok){return status;}if(!matches){continue;}
      const srl_xml_identity_attribute* attribute=NULL;if(field->path.attribute){for(size_t index=0;index<attribute_count;++index){if(strcmp(attributes[index].name.uri,field->path.attribute->uri)==0&&strcmp(attributes[index].name.local,field->path.attribute->local)==0){attribute=&attributes[index];break;}}if(!attribute){continue;}}
      if((constraint->kind==0u&&field->nillable)||candidate->seen[field_index]){return srl_invalid;}candidate->seen[field_index]=true;
      if(attribute){srl_string key={0};status=srl_xml_operation_identity_key(state,(const char*)attribute->value,(size_t)xmlStrlen(attribute->value),field,reader,&key);if(status==srl_ok){status=srl_xml_operation_identity_store(state,candidate,field_index,&key);}srl_string_free(&key);if(status!=srl_ok){return status;}}
      else{srl_xml_identity_capture* capture=&candidate->captures[field_index];capture->active=true;capture->depth=state->depth;
        for(size_t index=0;index<attribute_count;++index){if(strcmp(attributes[index].name.uri,"http://www.w3.org/2001/XMLSchema-instance")==0&&strcmp(attributes[index].name.local,"nil")==0){if(!srl_xml_boolean((const char*)attributes[index].value,&capture->nil)){return srl_invalid;}break;}}}
    }
  }return srl_ok;
}

/* Join only active scalar fields; text runs remain independently admitted before any capture allocation. */
static inline srl_status srl_xml_operation_identity_text(srl_xml_operation_identity_state* state,const char* text,size_t size) {
  srl_status status=srl_xml_operation_identity_spend(state,size);if(status!=srl_ok){return status;}
  for(size_t candidate=0;candidate<state->active_count;++candidate){srl_xml_identity_candidate* item=&state->active[candidate];for(size_t field=0;field<state->scope->constraints[item->constraint].count;++field){srl_xml_identity_capture* capture=&item->captures[field];if(!capture->active){continue;}
    if((status=srl_xml_operation_identity_spend(state,size))!=srl_ok){return status;}if(capture->nil&&size){return srl_invalid;}
    if(capture->text.size>state->limits.max_tuple_bytes||size>state->limits.max_tuple_bytes-capture->text.size){return srl_xml_operation_fail(state->operation,srl_limit);}
    if((status=srl_xml_operation_identity_put(state->operation,&capture->text,text,size))!=srl_ok){return status;}
  }}return srl_ok;
}

/* Close captures under their original namespace scope and admit complete tuples before indexing them. */
static inline srl_status srl_xml_operation_identity_end(srl_xml_operation_identity_state* state,xmlTextReaderPtr reader) {
  if(!state->depth||state->complete){return srl_invalid;}srl_status status=srl_xml_operation_identity_spend(state,1u);if(status!=srl_ok){return status;}
  size_t position=0;while(position<state->active_count){srl_xml_identity_candidate* candidate=&state->active[position];const srl_xml_identity_constraint* constraint=&state->scope->constraints[candidate->constraint];if(state->diagnostics){srl_xml_operation_select(state->operation,state->diagnostics[candidate->constraint].count?state->diagnostics[candidate->constraint].fields[0]:state->diagnostics[candidate->constraint].name);}
    for(size_t index=0;index<constraint->count;++index){srl_xml_identity_capture* capture=&candidate->captures[index];if(!capture->active||capture->depth!=state->depth){continue;}
      if(!capture->nil){srl_string value={0};status=srl_xml_operation_identity_key(state,(const char*)capture->text.data,capture->text.size,&constraint->fields[index],reader,&value);if(status==srl_ok){status=srl_xml_operation_identity_store(state,candidate,index,&value);}srl_string_free(&value);if(status!=srl_ok){return status;}}
      srl_buffer_free(&capture->text);capture->active=false;
    }
    if(candidate->depth!=state->depth){++position;continue;}bool complete=true;for(size_t index=0;index<constraint->count;++index){if(!candidate->values[index].data){complete=false;break;}}
    if(!complete){if(constraint->kind==0u){return srl_invalid;}}
    else{if(state->tuples>=state->limits.max_tuples||state->bytes>state->limits.max_tuple_bytes||candidate->bytes>state->limits.max_tuple_bytes-state->bytes){return srl_xml_operation_fail(state->operation,srl_limit);}
      if(state->diagnostics){srl_xml_operation_select(state->operation,state->diagnostics[candidate->constraint].count?state->diagnostics[candidate->constraint].fields[0]:state->diagnostics[candidate->constraint].name);}if(!srl_xml_operation_charge(state->operation,srl_xml_operation_references,1u)){return srl_invalid;}
      ++state->tuples;state->bytes+=candidate->bytes;srl_buffer tuple={0};
      for(size_t index=0;index<constraint->count&&status==srl_ok;++index){const srl_string* value=&candidate->values[index];char count[srl_number_scratch_bytes];const int written=snprintf(count,sizeof(count),"%zu:",value->size);
        if(written<0||(size_t)written>=sizeof(count)){status=srl_invalid;break;}status=srl_xml_operation_identity_put(state->operation,&tuple,count,(size_t)written);if(status==srl_ok){status=srl_xml_operation_identity_put(state->operation,&tuple,value->data,value->size);}}
      srl_string key={0};if(status==srl_ok){status=srl_xml_operation_string_set(state->operation,&key,(const char*)tuple.data,tuple.size);}srl_buffer_free(&tuple);
      if(status==srl_ok){status=srl_xml_operation_identity_insert(state,candidate->constraint,&key);}srl_string_free(&key);if(status!=srl_ok){return status;}
    }
    srl_xml_operation_identity_candidate_free(candidate,constraint->count);--state->active_count;if(position<state->active_count){memmove(candidate,candidate+1,(state->active_count-position)*sizeof(*candidate));}
  }
  --state->depth;srl_string_free(&state->path[state->depth].uri);srl_string_free(&state->path[state->depth].local);state->complete=state->depth==0;return srl_ok;
}

/* Complete deferred owner-local references before any typed owner or output bytes can be published. */
static inline srl_status srl_xml_operation_identity_finish(srl_xml_operation_identity_state* state) {
  if(!state->complete||state->depth||state->active_count){return srl_invalid;}
  if(state->diagnostics){for(size_t index=0;index<state->scope->count;++index){if(state->scope->constraints[index].kind==2u){const srl_xml_identity_diagnostic_constraint* diagnostic=&state->diagnostics[index];srl_xml_operation_select(state->operation,diagnostic->count?diagnostic->fields[0]:diagnostic->name);break;}}}
  srl_status status=srl_xml_operation_identity_spend(state,1u);if(status!=srl_ok){return status;}
  for(size_t index=0;index<state->scope->count;++index){const srl_xml_identity_constraint* constraint=&state->scope->constraints[index];if(constraint->kind!=2u){continue;}if(state->diagnostics){srl_xml_operation_select(state->operation,state->diagnostics[index].count?state->diagnostics[index].fields[0]:state->diagnostics[index].name);}
    for(size_t tuple=0;tuple<state->tables[index].count;++tuple){size_t slot=0;bool present=false;status=srl_xml_operation_identity_find(state,&state->tables[constraint->refer],&state->tables[index].values[tuple],&slot,&present);if(status!=srl_ok){return status;}if(!present){return srl_invalid;}}}
  return srl_ok;
}

/* Retain only this start's attributes; every value is released before advancing the provider. */
static inline srl_status srl_xml_operation_identity_attributes(srl_xml_input* input,srl_xml_identity_attribute** result,size_t* count) {
  srl_xml_operation* operation=srl_xml_operation_input_context(input->reader);*result=NULL;*count=0;if(input->member_attributes>SIZE_MAX/sizeof((*result)[0])){return srl_xml_operation_fail(operation,srl_limit);}
  if(input->member_attributes){if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,input->member_attributes*sizeof((*result)[0]))){return srl_invalid;}*result=(srl_xml_identity_attribute*)calloc(input->member_attributes,sizeof((*result)[0]));if(!*result){return srl_allocation;}}
  srl_status status=srl_ok;if(xmlTextReaderMoveToFirstAttribute(input->reader)==1){do{
    if(xmlTextReaderIsNamespaceDecl(input->reader)){continue;}if(*count>=input->member_attributes){status=srl_invalid;break;}
    srl_xml_identity_attribute* attribute=&(*result)[(*count)++];const xmlChar* uri=xmlTextReaderConstNamespaceUri(input->reader);
    attribute->name.uri=uri?(const char*)uri:"";attribute->name.local=(const char*)xmlTextReaderConstLocalName(input->reader);if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,(size_t)xmlStrlen(xmlTextReaderConstValue(input->reader))+1u)){status=srl_invalid;break;}attribute->value=xmlTextReaderValue(input->reader);
    if(!attribute->value){status=srl_allocation;break;}
  }while(xmlTextReaderMoveToNextAttribute(input->reader)==1);xmlTextReaderMoveToElement(input->reader);}return status;
})copidentity"
R"copidentity(

/* Validate complete original/final XML without document nodes or native owner copies; failures discard every private index. */
static inline srl_status srl_xml_operation_identity_validate(const uint8_t* bytes,size_t size,const srl_limits* limits,
    const srl_xml_identity_limits* identity_limits,const srl_xml_identity_node* graph,size_t graph_count,const srl_xml_identity_diagnostic_node* diagnostics,srl_xml_operation* operation) {
  bool enabled=false;for(size_t index=0;index<graph_count;++index){if(graph[index].scope){enabled=true;break;}}if(!enabled){return srl_ok;}
  srl_xml_operation_reader reader_operation={0};reader_operation.operation=operation;reader_operation.graph=graph;reader_operation.diagnostics=diagnostics;reader_operation.graph_count=graph_count;reader_operation.secondary=true;
  srl_xml_input input;srl_status status=srl_xml_operation_input_init(&input,bytes,size,limits,operation?&reader_operation:NULL);if(status!=srl_ok){srl_xml_input_free(&input);free(reader_operation.path);return status;}
  size_t* path=NULL;size_t depth=0,path_capacity=0;srl_xml_operation_identity_state* scopes=NULL;size_t scope_count=0,scope_capacity=0;
  while(status==srl_ok&&input.kind!=XML_READER_TYPE_NONE){
    if(input.kind==XML_READER_TYPE_ELEMENT){srl_xml_identity_attribute* attributes=NULL;size_t attribute_count=0;status=srl_xml_operation_identity_attributes(&input,&attributes,&attribute_count);
      const xmlChar* uri=xmlTextReaderConstNamespaceUri(input.reader);const srl_xml_identity_name name={uri?(const char*)uri:"",(const char*)xmlTextReaderConstLocalName(input.reader)};
      size_t current=depth?SIZE_MAX:0;
      if(status==srl_ok&&depth&&path[depth-1u]!=SIZE_MAX){status=srl_xml_identity_edge_target(&graph[path[depth-1u]],&name,attributes,attribute_count,input.reader,&current);}
      if(status==srl_ok){void* allocation=path;status=srl_xml_operation_identity_grow(operation,&allocation,&path_capacity,depth+1u,sizeof(path[0]));path=(size_t*)allocation;}
      if(status==srl_ok){path[depth++]=current;if(current!=SIZE_MAX&&graph[current].scope){void* allocation=scopes;status=srl_xml_operation_identity_grow(operation,&allocation,&scope_capacity,scope_count+1u,sizeof(scopes[0]));scopes=(srl_xml_operation_identity_state*)allocation;
          if(status==srl_ok){status=srl_xml_operation_identity_init(&scopes[scope_count],graph[current].scope,identity_limits,depth,operation,diagnostics?diagnostics[current].constraints:NULL);if(status==srl_ok){++scope_count;}else{srl_xml_operation_identity_free(&scopes[scope_count]);}}}}
      for(size_t scope=0;scope<scope_count&&status==srl_ok;++scope){status=srl_xml_operation_identity_start(&scopes[scope],&name,attributes,attribute_count,input.reader);}
      srl_xml_identity_attributes_free(attributes,attribute_count);
    }else if(input.kind==XML_READER_TYPE_END_ELEMENT){
      if(!depth){status=srl_invalid;break;}for(size_t scope=0;scope<scope_count&&status==srl_ok;++scope){status=srl_xml_operation_identity_end(&scopes[scope],input.reader);}
      if(status==srl_ok&&scope_count&&scopes[scope_count-1u].owner_depth==depth){status=srl_xml_operation_identity_finish(&scopes[scope_count-1u]);if(status==srl_ok){srl_xml_operation_identity_free(&scopes[--scope_count]);}}
      --depth;
    }else if(input.kind==XML_READER_TYPE_TEXT||input.kind==XML_READER_TYPE_CDATA||input.kind==XML_READER_TYPE_WHITESPACE||input.kind==XML_READER_TYPE_SIGNIFICANT_WHITESPACE){
      const xmlChar* value=xmlTextReaderConstValue(input.reader);const size_t count=value?(size_t)xmlStrlen(value):0;
      for(size_t scope=0;scope<scope_count&&status==srl_ok;++scope){status=srl_xml_operation_identity_text(&scopes[scope],value?(const char*)value:"",count);}
    }
    if(status==srl_ok&&!srl_xml_input_next(&input)){status=input.status==srl_ok?srl_invalid:input.status;}
  }
  if(status==srl_ok&&(depth||scope_count)){status=srl_invalid;}
  for(size_t scope=0;scope<scope_count;++scope){srl_xml_operation_identity_free(&scopes[scope]);}free(scopes);free(path);srl_xml_input_free(&input);free(reader_operation.path);return status;
})copidentity"
