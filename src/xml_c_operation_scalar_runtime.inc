// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"copscalar(/* Additive scalar work state keeps the old public scalar helper aggregate intact. */
typedef struct srl_xml_operation_scalar_work {size_t remaining,max_items;srl_xml_operation* operation;} srl_xml_operation_scalar_work;
/* Admit each owned spelling before replacing initialized native string storage transactionally. */
static inline srl_status srl_xml_operation_string_set(srl_xml_operation* operation,srl_string* result,const char* text,size_t size) {
  if(!operation){return srl_string_set(result,text,size);}
  if(size==SIZE_MAX){return srl_xml_operation_fail(operation,srl_limit);}
  if(!srl_xml_operation_utf8(operation,(const uint8_t*)text,size)){return srl_xml_operation_fail(operation,srl_invalid);}
  if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,size+1u)){return srl_invalid;}
  char* copy=(char*)malloc(size+1u);if(!copy){return srl_xml_operation_fail(operation,srl_allocation);}
  const size_t interval_bytes=1024u;
  for(size_t offset=0;offset<size;){const size_t count=size-offset<interval_bytes?size-offset:interval_bytes;
    if(!srl_xml_operation_charge(operation,srl_xml_operation_work,count)){free(copy);return srl_invalid;}memcpy(copy+offset,text+offset,count);offset+=count;}
  copy[size]=0;srl_string_free(result);result->data=copy;result->size=size;return srl_ok;
}
/* Admit instruction visits and lexical scans before performing their work. */
static inline srl_status srl_xml_operation_scalar_spend(srl_xml_operation_scalar_work* work, size_t units) {
  if(!srl_xml_operation_charge(work->operation,srl_xml_operation_work,units)){return srl_invalid;}
  if (units > work->remaining) { return srl_xml_operation_fail(work->operation,srl_limit); } work->remaining -= units; return srl_ok;
}

/* Count items before allocating lexical slices or application list storage. */
static inline srl_status srl_xml_operation_scalar_count(srl_xml_operation* operation,const char* text, size_t maximum, size_t* result) {
  size_t count=0;bool item=false;
  for (const unsigned char* cursor=(const unsigned char*)text;*cursor;++cursor) {
    if(!srl_xml_operation_charge(operation,srl_xml_operation_work,1u)){return srl_invalid;}const bool space=srl_xml_scalar_space(*cursor); if (!space&&!item) { if (count>=maximum) { return srl_xml_operation_fail(operation,srl_limit); } ++count; } item=!space;
  } *result=count;return srl_ok;
}

/* Validate XML/UTF8 before making a bounded owned NUL-terminated lexical candidate. */
static inline srl_status srl_xml_operation_scalar_normalize(srl_xml_operation* operation,const char* raw, size_t size, const char* mode, srl_string* result) {
  if ((size&&!raw)||size==SIZE_MAX||!srl_xml_operation_characters(operation,raw,size)) { return srl_invalid; }
  if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,size+1u)){return srl_invalid;}
  char* text=(char*)malloc(size+1u);if (!text) { return srl_allocation; } size_t count=0;bool pending=false;
  const bool collapse=strcmp(mode,"collapse")==0,replace=collapse||strcmp(mode,"replace")==0;
  for (size_t index=0;index<size;++index) {
    if(index%1024u==0u&&!srl_xml_operation_charge(operation,srl_xml_operation_work,size-index<1024u?size-index:1024u)){free(text);return srl_invalid;}
    char value=raw[index];if (replace&&srl_xml_scalar_space((unsigned char)value)) { value=' '; }
    if (collapse&&value==' ') { pending=count!=0;continue; }
    if (pending) { text[count++]=' ';pending=false; } text[count++]=value;
  } text[count]=0;srl_string_free(result);result->data=text;result->size=count;return srl_ok;
}

/* Deduplicate when scheduling, so nullable cycles fit the same fixed pending stack as consuming states. */
static inline srl_status srl_xml_operation_scalar_closure(const srl_xml_scalar_pattern* pattern, size_t* selected,
    size_t* count, bool* visited, size_t* stack, srl_xml_operation_scalar_work* work) {
  srl_status status=srl_xml_operation_scalar_spend(work,pattern->state_count);if (status!=srl_ok) { return status; }
  memset(visited,0,pattern->state_count*sizeof(*visited));size_t pending=0,kept=0;
  for (size_t index=0;index<*count;++index) {
    if ((status=srl_xml_operation_scalar_spend(work,1u))!=srl_ok) { return status; }
    const size_t target=selected[index];if (!visited[target]) { visited[target]=true;stack[pending++]=target; }
  }
  while (pending) {
    if ((status=srl_xml_operation_scalar_spend(work,1u))!=srl_ok) { return status; }
    const size_t current=stack[--pending];const srl_xml_scalar_state* state=&pattern->states[current];
    if (state->flags&9u) { selected[kept++]=current; }
    for (size_t index=0;index<state->epsilon_count;++index) {
      if ((status=srl_xml_operation_scalar_spend(work,1u))!=srl_ok) { return status; }
      const size_t target=state->epsilon[index];if (!visited[target]) { visited[target]=true;stack[pending++]=target; }
    }
  } *count=kept;return srl_ok;
}

/* Run the compiler-owned Thompson program over Unicode, with finite input and operation work. */
static inline srl_status srl_xml_operation_scalar_match(const srl_xml_scalar_pattern* pattern, const char* text,
    srl_xml_operation_scalar_work* work, bool* result) {
  const size_t size=strlen(text),states=pattern->state_count;
  if (size>srl_xml_scalar_pattern_bytes||states>SIZE_MAX/(3u*sizeof(size_t)+sizeof(bool))) { return srl_xml_operation_fail(work->operation,srl_limit); }
  if (!states) { return srl_invalid; }
  if(!srl_xml_operation_charge(work->operation,srl_xml_operation_allocation,states*(3u*sizeof(size_t)+sizeof(bool)))){return srl_invalid;}
  void* allocation=malloc(states*(3u*sizeof(size_t)+sizeof(bool)));if (!allocation) { return srl_allocation; }
  size_t* active=(size_t*)allocation;size_t* next=active+states;size_t* stack=next+states;bool* visited=(bool*)(stack+states);
  active[0]=pattern->start;size_t count=1,offset=0;srl_status status=srl_xml_operation_scalar_closure(pattern,active,&count,visited,stack,work);
  while (status==srl_ok&&offset<size) {
    const uint32_t value=srl_xml_scalar_code(text,&offset);size_t following=0;
    for (size_t index=0;index<count;++index) {
      if ((status=srl_xml_operation_scalar_spend(work,1u))!=srl_ok) { break; }
      const srl_xml_scalar_state* state=&pattern->states[active[index]];if (!(state->flags&1u)) { continue; } bool matched=false;
      if (state->flags&4u) { matched=value!=10u&&value!=13u; }
      else {
        for (size_t range=0;range<state->range_count;++range) {
          if ((status=srl_xml_operation_scalar_spend(work,1u))!=srl_ok) { break; }
          if (value>=state->ranges[range].first&&value<=state->ranges[range].last) { matched=true; }
        } if (state->flags&2u) { matched=!matched; }
      }
      if (matched) { next[following++]=state->next; }
    }
    if (status!=srl_ok) { break; } count=following;status=srl_xml_operation_scalar_closure(pattern,next,&count,visited,stack,work);
    size_t* exchanged=active;active=next;next=exchanged;
  }
  *result=false;if (status==srl_ok) { for (size_t index=0;index<count;++index) { if (pattern->states[active[index]].flags&8u) { *result=true;break; } } }
  free(allocation);return status;
}

/* Match list values itemwise rather than comparing their equivalent numeric/Boolean spellings as strings. */
static inline srl_status srl_xml_operation_scalar_compare(const srl_xml_scalar_rule* rule, const char* left, const char* right,
    bool equal_nan, srl_xml_operation_scalar_work* work, int* order) {
  if (!*rule->list_kind) { return srl_xml_scalar_compare_kind(rule->kind,left,right,equal_nan,order); }
  size_t left_count=0,right_count=0;srl_status status=srl_xml_operation_scalar_count(work->operation,left,work->max_items,&left_count);
  if (status==srl_ok) { status=srl_xml_operation_scalar_count(work->operation,right,work->max_items,&right_count); } if (status!=srl_ok) { return status; }
  *order=1;if (left_count!=right_count) { return srl_ok; }
  size_t a_offset=0,b_offset=0,a_begin=0,b_begin=0,a_size=0,b_size=0;
  while (srl_xml_scalar_token(left,&a_offset,&a_begin,&a_size)&&srl_xml_scalar_token(right,&b_offset,&b_begin,&b_size)) {
    if (a_size>SIZE_MAX-b_size||(status=srl_xml_operation_scalar_spend(work,a_size+b_size))!=srl_ok) { return srl_xml_operation_fail(work->operation,srl_limit); }
    srl_string a={0},b={0};status=srl_xml_operation_string_set(work->operation,&a,left+a_begin,a_size);
    if (status==srl_ok) { status=srl_xml_operation_string_set(work->operation,&b,right+b_begin,b_size); }
    if (status==srl_ok) { status=srl_xml_scalar_compare_kind(rule->list_kind,a.data,b.data,equal_nan,order); }
    srl_string_free(&a);srl_string_free(&b);if (status!=srl_ok||*order!=0) { return status; }
  } *order=0;return srl_ok;
}

/* Validate one normalized candidate, retaining resource failures across independent union alternatives. */
static inline srl_status srl_xml_operation_scalar_check_work(const srl_xml_scalar_rule* rule, const char* raw, size_t size,
    srl_xml_operation_scalar_work* work, srl_string* result) {
  srl_status status=srl_xml_operation_scalar_spend(work,size);if (status!=srl_ok) { return status; } srl_string text={0};
  status=srl_xml_operation_scalar_normalize(work->operation,raw,size,rule->whitespace,&text);if (status!=srl_ok) { return status; }
  status=srl_xml_scalar_lexical(rule->kind,text.data);if (status!=srl_ok) { goto done; }
  if (rule->member_count) {
    bool matched=false;
    for (size_t index=0;index<rule->member_count;++index) {
      srl_string alternative={0};status=srl_xml_operation_scalar_check_work(&rule->members[index],text.data,text.size,work,&alternative);srl_string_free(&alternative);
      if (status==srl_ok) { matched=true;break; } if (status==srl_limit||status==srl_allocation) { goto done; }
    } if (!matched) { status=srl_invalid;goto done; }
  }
  size_t count=0;
  if (*rule->list_kind) {
    status=srl_xml_operation_scalar_count(work->operation,text.data,work->max_items,&count);if (status!=srl_ok) { goto done; }
    size_t offset=0,begin=0,item_size=0;
    while (srl_xml_scalar_token(text.data,&offset,&begin,&item_size)) {
      srl_string item={0};status=srl_xml_operation_scalar_spend(work,item_size);
      if (status==srl_ok) { status=srl_xml_operation_string_set(work->operation,&item,text.data+begin,item_size); }
      if (status==srl_ok) { status=srl_xml_scalar_lexical(rule->list_kind,item.data); }
      srl_string_free(&item);if (status!=srl_ok) { goto done; }
    }
  } else { size_t offset=0;while (offset<text.size) { srl_xml_scalar_code(text.data,&offset);++count; } }
  if ((rule->length!=SIZE_MAX&&count!=rule->length)||(rule->minimum!=SIZE_MAX&&count<rule->minimum)||
      (rule->maximum!=SIZE_MAX&&count>rule->maximum)) { status=srl_invalid;goto done; }
  for (size_t index=0;index<sizeof(rule->bounds)/sizeof(rule->bounds[0]);++index) {
    if (!*rule->bounds[index]) { continue; } int order=0;status=srl_xml_operation_scalar_compare(rule,text.data,rule->bounds[index],false,work,&order);
    if (status!=srl_ok) { goto done; }
    if (order==2||((index==0u||index==2u)&&order<0)||((index==1u||index==3u)&&order>0)||(index>=2u&&order==0)) { status=srl_invalid;goto done; }
  }
  if (rule->enumeration_count) {
    bool matched=false;
    for (size_t index=0;index<rule->enumeration_count;++index) {
      srl_string expected={0};int order=0;status=srl_xml_operation_scalar_normalize(work->operation,rule->enumeration[index],strlen(rule->enumeration[index]),rule->whitespace,&expected);
      if (status==srl_ok) { status=srl_xml_operation_scalar_compare(rule,text.data,expected.data,true,work,&order); } srl_string_free(&expected);
      if (status!=srl_ok) { goto done; } if (!order) { matched=true;break; }
    } if (!matched) { status=srl_invalid;goto done; }
  }
  if (rule->fixed) {
    srl_string expected={0};int order=0;status=srl_xml_operation_scalar_normalize(work->operation,rule->fixed,strlen(rule->fixed),rule->whitespace,&expected);
    if (status==srl_ok) { status=srl_xml_operation_scalar_compare(rule,text.data,expected.data,true,work,&order); } srl_string_free(&expected);
    if (status!=srl_ok) { goto done; } if (order) { status=srl_invalid;goto done; }
  }
  if (rule->finite) { double number=0;if (!srl_xml_float(text.data,strcmp(rule->kind,"float")==0,&number)||!isfinite(number)) { status=srl_invalid;goto done; } }
  for (size_t group=0;group<rule->group_count;++group) {
    bool matched=false;
    for (size_t index=0;index<rule->groups[group].count;++index) {
      status=srl_xml_operation_scalar_match(&rule->groups[group].patterns[index],text.data,work,&matched);
      if (status!=srl_ok) { goto done; } if (matched) { break; }
    } if (!matched) { status=srl_invalid;goto done; }
  }
done:
  if (status==srl_ok) { srl_string_free(result);*result=text; } else { srl_string_free(&text); } return status;
}

/* Start independent scalar work only after document admission has bounded the raw text. */
static inline srl_status srl_xml_operation_scalar_check(srl_xml_operation* operation,const srl_xml_scalar_rule* rule, const char* raw, size_t size,
    size_t maximum_items, srl_string* result) {
  srl_xml_operation_scalar_work work={srl_xml_scalar_max_work,maximum_items,operation};return srl_xml_operation_scalar_check_work(rule,raw,size,&work,result);
})copscalar"
R"copscalar(

/* Preserve an explicit normalized empty collapse value instead of reactivating a different element default. */
static inline srl_status srl_xml_operation_scalar_output(srl_xml_operation* operation,const srl_xml_scalar_rule* rule, const char* raw, size_t size,
    const char* fallback, size_t maximum_items, srl_string* result) {
  srl_status status=srl_xml_operation_scalar_check(operation,rule,raw,size,maximum_items,result);if (status!=srl_ok||result->size||!fallback) { return status; }
  srl_string expected={0};status=srl_xml_operation_scalar_normalize(operation,fallback,strlen(fallback),rule->whitespace,&expected);
  if (status==srl_ok&&expected.size) { status=strcmp(rule->whitespace,"collapse")==0?srl_xml_operation_string_set(operation,result," ",1u):srl_invalid; }
  srl_string_free(&expected);return status;
}

/* Append one native list item, checking separators, item spelling and text admission before reallocation. */
static inline srl_status srl_xml_operation_scalar_append(srl_xml_operation* operation,srl_buffer* joined, const char* text, size_t size, size_t maximum) {
  if (!size) { return srl_invalid; } for (size_t index=0;index<size;++index) { if (srl_xml_scalar_space((unsigned char)text[index])) { return srl_invalid; } }
  const size_t separator=joined->size?1u:0u;if (joined->size>maximum||separator>maximum-joined->size||size>maximum-joined->size-separator) { return srl_xml_operation_fail(operation,srl_limit); }
  if (size>SIZE_MAX-joined->size-separator) { return srl_xml_operation_fail(operation,srl_limit); } const size_t count=joined->size+separator+size;
  if(!srl_xml_operation_charge(operation,srl_xml_operation_work,size)||!srl_xml_operation_charge(operation,srl_xml_operation_allocation,separator+size)){return srl_invalid;}if (!srl_reserve(joined,separator+size)) { return srl_allocation; }
  if (separator) { joined->data[joined->size++]=' '; } memcpy(joined->data+joined->size,text,size);joined->size=count;return srl_ok;
}

/* Admit exact hex before allocation and replace an owning buffer only on complete success. */
static inline srl_status srl_xml_operation_digest_read(srl_xml_operation* operation,const char* text,size_t size,size_t extent,
    srl_xml_digest_limits limits,srl_buffer* result) {
  const size_t count=size/2u;
  if (size%2u || (extent && count != extent)) { return srl_invalid; }
  if (count > limits.max_bytes) { return srl_xml_operation_fail(operation,srl_limit); }
  for (size_t index=0; index < size; ++index) {if(index%1024u==0u&&!srl_xml_operation_charge(operation,srl_xml_operation_work,size-index<1024u?size-index:1024u)){return srl_invalid;} if (srl_xml_digest_digit((unsigned char)text[index]) < 0) { return srl_invalid; } }
  if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,count)){return srl_invalid;}srl_buffer candidate={0}; candidate.data=count ? (uint8_t*)malloc(count) : NULL;
  if (count && !candidate.data) { return srl_allocation; } candidate.size=count; candidate.capacity=count;
  for (size_t index=0; index < count; ++index) { if(index%1024u==0u&&!srl_xml_operation_charge(operation,srl_xml_operation_work,count-index<1024u?count-index:1024u)){srl_buffer_free(&candidate);return srl_invalid;}candidate.data[index]=(uint8_t)(srl_xml_digest_digit((unsigned char)text[index*2u])*16+srl_xml_digest_digit((unsigned char)text[index*2u+1u])); }
  srl_buffer_free(result); *result=candidate; return srl_ok;
}

/* Produce canonical lowercase hex from borrowed native storage after independent size admission. */
static inline srl_status srl_xml_operation_digest_text(srl_xml_operation* operation,const srl_buffer* value,size_t extent,
    srl_xml_digest_limits limits,srl_buffer* result) {
  if ((value->size && !value->data) || (extent && value->size != extent)) { return srl_invalid; }
  if (value->size > limits.max_bytes || value->size > (SIZE_MAX-1u)/2u) { return srl_xml_operation_fail(operation,srl_limit); }
  const size_t size=value->size*2u; if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,size+1u)){return srl_invalid;}result->data=(uint8_t*)malloc(size+1u); if (!result->data) { return srl_allocation; }
  result->size=size; result->capacity=size+1u; const char* hex="0123456789abcdef";
  for (size_t index=0; index < value->size; ++index) { if(index%1024u==0u&&!srl_xml_operation_charge(operation,srl_xml_operation_work,value->size-index<1024u?value->size-index:1024u)){srl_buffer_free(result);return srl_invalid;}result->data[index*2u]=(uint8_t)hex[value->data[index]>>4]; result->data[index*2u+1u]=(uint8_t)hex[value->data[index]&15u]; }
  result->data[size]=0; return srl_ok;
})copscalar"
