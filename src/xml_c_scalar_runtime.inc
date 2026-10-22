// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"cscalar(
/* Immutable compiler-owned Unicode instructions avoid platform regex dialects and runtime preparation. */
typedef struct srl_xml_scalar_range { uint32_t first, last; } srl_xml_scalar_range;
typedef struct srl_xml_scalar_state {
  const size_t* epsilon; size_t epsilon_count; const srl_xml_scalar_range* ranges; size_t range_count, next; unsigned flags;
} srl_xml_scalar_state;
typedef struct srl_xml_scalar_pattern { size_t start; const srl_xml_scalar_state* states; size_t state_count; } srl_xml_scalar_pattern;
typedef struct srl_xml_scalar_group { const srl_xml_scalar_pattern* patterns; size_t count; } srl_xml_scalar_group;
/* Schema literal identities retain expanded names rather than namespace prefix spellings. */
typedef struct srl_xml_scalar_expanded_name { const char *uri, *local; } srl_xml_scalar_expanded_name;
typedef struct srl_xml_scalar_identity {
  const char *kind, *list_kind, *lexical; const srl_xml_scalar_expanded_name* names; size_t name_count;
  const struct srl_xml_scalar_identity* items; size_t item_count;
} srl_xml_scalar_identity;
/* SIZE_MAX represents an unspecified length; every pointer/count pair refers to immutable generated data. */
typedef struct srl_xml_scalar_rule {
  const char *kind, *whitespace; const char* bounds[4]; size_t length, minimum, maximum;
  const char* const* enumeration; size_t enumeration_count; bool finite;
  const srl_xml_scalar_group* groups; size_t group_count; const char* list_kind;
  const struct srl_xml_scalar_rule* members; size_t member_count; const char* fixed;
  bool namespace_dependent; const srl_xml_scalar_identity* identities; size_t identity_count;
  const srl_xml_scalar_identity* fixed_identity;
  const struct srl_xml_scalar_rule* item_rule;
  const char* schema_default;
} srl_xml_scalar_rule;
typedef struct srl_xml_scalar_work { size_t remaining, max_items; } srl_xml_scalar_work;
/* Admit instruction visits and lexical scans before performing their work. */
static inline srl_status srl_xml_scalar_spend(srl_xml_scalar_work* work, size_t units) {
  if (units > work->remaining) { return srl_limit; } work->remaining -= units; return srl_ok;
}
/* Recognize only the four XML whitespace bytes, independently of the process locale. */
static inline bool srl_xml_scalar_space(unsigned char value) { return value==' ' || value=='\t' || value=='\r' || value=='\n'; }
/* Read one already validated UTF8 scalar; callers validate the complete input before invoking this cursor. */
static inline uint32_t srl_xml_scalar_code(const char* text, size_t* offset) {
  const unsigned char first=(unsigned char)text[(*offset)++]; uint32_t value=first; unsigned count=0;
  if (first>=0xf0u) { value=first&7u;count=3; } else if (first>=0xe0u) { value=first&15u;count=2; }
  else if (first>=0xc0u) { value=first&31u;count=1; }
  for (unsigned index=0;index<count;++index) { value=(value<<6)|((unsigned char)text[(*offset)++]&63u); } return value;
}
/* Count items before allocating lexical slices or application list storage. */
static inline srl_status srl_xml_scalar_count(const char* text, size_t maximum, size_t* result) {
  size_t count=0;bool item=false;
  for (const unsigned char* cursor=(const unsigned char*)text;*cursor;++cursor) {
    const bool space=srl_xml_scalar_space(*cursor); if (!space&&!item) { if (count>=maximum) { return srl_limit; } ++count; } item=!space;
  } *result=count;return srl_ok;
}
/* Return the next logical token without allocating or reading past the terminating byte. */
static inline bool srl_xml_scalar_token(const char* text, size_t* offset, size_t* begin, size_t* size) {
  while (srl_xml_scalar_space((unsigned char)text[*offset])) { ++*offset; } if (!text[*offset]) { return false; }
  *begin=*offset;while (text[*offset]&&!srl_xml_scalar_space((unsigned char)text[*offset])) { ++*offset; } *size=*offset-*begin;return true;
}
/* Validate XML/UTF8 before making a bounded owned NUL-terminated lexical candidate. */
static inline srl_status srl_xml_scalar_normalize(const char* raw, size_t size, const char* mode, srl_string* result) {
  if ((size&&!raw)||!srl_xml_characters(raw,size)||size==SIZE_MAX) { return srl_invalid; }
  char* text=(char*)malloc(size+1u);if (!text) { return srl_allocation; } size_t count=0;bool pending=false;
  const bool collapse=strcmp(mode,"collapse")==0,replace=collapse||strcmp(mode,"replace")==0;
  for (size_t index=0;index<size;++index) {
    char value=raw[index];if (replace&&srl_xml_scalar_space((unsigned char)value)) { value=' '; }
    if (collapse&&value==' ') { pending=count!=0;continue; }
    if (pending) { text[count++]=' ';pending=false; } text[count++]=value;
  } text[count]=0;srl_string_free(result);result->data=text;result->size=count;return srl_ok;
}
/* Require complete decimal/integer grammar before exact digit comparison. */
static inline bool srl_xml_scalar_digits(const char* text, bool decimal) {
  if (*text=='+'||*text=='-') { ++text; } bool dot=false,seen=false;
  for (;*text;++text) { if (*text>='0'&&*text<='9') { seen=true; } else if (decimal&&*text=='.'&&!dot) { dot=true; } else { return false; } }
  return seen;
}
typedef struct srl_xml_scalar_decimal { bool negative; const char *whole,*fraction; size_t whole_size,fraction_size; } srl_xml_scalar_decimal;
/* Remove insignificant zeros while retaining borrowed slices and an exact value sign. */
static inline srl_xml_scalar_decimal srl_xml_scalar_decimal_parts(const char* text) {
  srl_xml_scalar_decimal result={0};result.negative=*text=='-';if (*text=='-'||*text=='+') { ++text; }
  const char* dot=strchr(text,'.');result.whole=text;result.whole_size=dot?(size_t)(dot-text):strlen(text);
  while (result.whole_size&&*result.whole=='0') { ++result.whole;--result.whole_size; }
  result.fraction=dot?dot+1:"";result.fraction_size=strlen(result.fraction);
  while (result.fraction_size&&result.fraction[result.fraction_size-1u]=='0') { --result.fraction_size; }
  result.negative=result.negative&&(result.whole_size||result.fraction_size);return result;
}
/* Compare arbitrary decimal magnitudes without binary floating-point conversion. */
static inline int srl_xml_scalar_decimal_compare(const char* left, const char* right) {
  const srl_xml_scalar_decimal a=srl_xml_scalar_decimal_parts(left),b=srl_xml_scalar_decimal_parts(right);
  if (a.negative!=b.negative) { return a.negative?-1:1; } int order=0;
  if (a.whole_size!=b.whole_size) { order=a.whole_size<b.whole_size?-1:1; }
  else if (a.whole_size) { const int compared=memcmp(a.whole,b.whole,a.whole_size);order=(compared>0)-(compared<0); }
  for (size_t index=0;order==0&&(index<a.fraction_size||index<b.fraction_size);++index) {
    const char first=index<a.fraction_size?a.fraction[index]:'0',second=index<b.fraction_size?b.fraction[index]:'0';order=(first>second)-(first<second);
  } return a.negative?-order:order;
}
/* Use the XML fifth-edition ranges for Name/NCName, including combining characters after the first scalar. */
static inline bool srl_xml_scalar_name(uint32_t value, bool first, bool colon) {
  const bool start=value==0x5fu||(colon&&value==0x3au)||(value>=0x41u&&value<=0x5au)||(value>=0x61u&&value<=0x7au)||
    (value>=0xc0u&&value<=0xd6u)||(value>=0xd8u&&value<=0xf6u)||(value>=0xf8u&&value<=0x2ffu)||
    (value>=0x370u&&value<=0x37du)||(value>=0x37fu&&value<=0x1fffu)||(value>=0x200cu&&value<=0x200du)||
    (value>=0x2070u&&value<=0x218fu)||(value>=0x2c00u&&value<=0x2fefu)||(value>=0x3001u&&value<=0xd7ffu)||
    (value>=0xf900u&&value<=0xfdcfu)||(value>=0xfdf0u&&value<=0xfffdu)||(value>=0x10000u&&value<=0xeffffu);
  return start||(!first&&(value==0x2du||value==0x2eu||value==0xb7u||(value>=0x30u&&value<=0x39u)||
      (value>=0x300u&&value<=0x36fu)||(value>=0x203fu&&value<=0x2040u)));
}
/* Classify integer contracts independently of native application field widths. */
static inline bool srl_xml_scalar_integer(const char* kind) {
  return strcmp(kind,"byte")==0||strcmp(kind,"unsignedByte")==0||strcmp(kind,"short")==0||strcmp(kind,"unsignedShort")==0||
    strcmp(kind,"int")==0||strcmp(kind,"unsignedInt")==0||strcmp(kind,"long")==0||strcmp(kind,"unsignedLong")==0||
    strcmp(kind,"integer")==0||strcmp(kind,"positiveInteger")==0||strcmp(kind,"negativeInteger")==0||
    strcmp(kind,"nonPositiveInteger")==0||strcmp(kind,"nonNegativeInteger")==0;
}
/* Validate builtin value spaces before custom facets or application conversion. */
static inline srl_status srl_xml_scalar_lexical(const char* kind, const char* text) {
  if (srl_xml_scalar_integer(kind)) {
    if (!srl_xml_scalar_digits(text,false)) { return srl_invalid; }
    static const char* const kinds[]={"byte","unsignedByte","short","unsignedShort","int","unsignedInt","long","unsignedLong"};
    static const char* const minimum[]={"-128","0","-32768","0","-2147483648","0","-9223372036854775808","0"};
    static const char* const maximum[]={"127","255","32767","65535","2147483647","4294967295","9223372036854775807","18446744073709551615"};
    for (size_t index=0;index<sizeof(kinds)/sizeof(kinds[0]);++index) {
      if (strcmp(kind,kinds[index])==0&&(srl_xml_scalar_decimal_compare(text,minimum[index])<0||srl_xml_scalar_decimal_compare(text,maximum[index])>0)) { return srl_invalid; }
    }
    const int sign=srl_xml_scalar_decimal_compare(text,"0");
    if ((strcmp(kind,"positiveInteger")==0&&sign<=0)||(strcmp(kind,"negativeInteger")==0&&sign>=0)||
        (strcmp(kind,"nonPositiveInteger")==0&&sign>0)||(strcmp(kind,"nonNegativeInteger")==0&&sign<0)) { return srl_invalid; }
  } else if (strcmp(kind,"decimal")==0) { if (!srl_xml_scalar_digits(text,true)) { return srl_invalid; } }
  else if (strcmp(kind,"boolean")==0) { bool value=false;if (!srl_xml_boolean(text,&value)) { return srl_invalid; } }
  else if (strcmp(kind,"float")==0||strcmp(kind,"double")==0) { double value=0;if (!srl_xml_float(text,strcmp(kind,"float")==0,&value)) { return srl_invalid; } }
  else if (strcmp(kind,"language")==0) {
    bool first=true;size_t count=0;
    for (const unsigned char* cursor=(const unsigned char*)text;;++cursor) {
      const unsigned char value=*cursor;if (!value||value=='-') { if (!count||count>8u) { return srl_invalid; } count=0;first=false;if (!value) { break; } }
      else { if (!((value>='A'&&value<='Z')||(value>='a'&&value<='z')||(!first&&value>='0'&&value<='9'))) { return srl_invalid; } ++count; }
    }
  } else if (strcmp(kind,"Name")==0||strcmp(kind,"NCName")==0||strcmp(kind,"NMTOKEN")==0||strcmp(kind,"ID")==0||strcmp(kind,"IDREF")==0||strcmp(kind,"ENTITY")==0) {
    if (!*text) { return srl_invalid; } size_t offset=0;bool first=strcmp(kind,"NMTOKEN")!=0;const bool colon=strcmp(kind,"Name")==0||strcmp(kind,"NMTOKEN")==0;
    while (text[offset]) { if (!srl_xml_scalar_name(srl_xml_scalar_code(text,&offset),first,colon)) { return srl_invalid; } first=false; }
  } return srl_ok;
}
)cscalar"
R"cscalar(
/* Deduplicate when scheduling, so nullable cycles fit the same fixed pending stack as consuming states. */
static inline srl_status srl_xml_scalar_closure(const srl_xml_scalar_pattern* pattern, size_t* selected,
    size_t* count, bool* visited, size_t* stack, srl_xml_scalar_work* work) {
  srl_status status=srl_xml_scalar_spend(work,pattern->state_count);if (status!=srl_ok) { return status; }
  memset(visited,0,pattern->state_count*sizeof(*visited));size_t pending=0,kept=0;
  for (size_t index=0;index<*count;++index) {
    if ((status=srl_xml_scalar_spend(work,1u))!=srl_ok) { return status; }
    const size_t target=selected[index];if (!visited[target]) { visited[target]=true;stack[pending++]=target; }
  }
  while (pending) {
    if ((status=srl_xml_scalar_spend(work,1u))!=srl_ok) { return status; }
    const size_t current=stack[--pending];const srl_xml_scalar_state* state=&pattern->states[current];
    if (state->flags&9u) { selected[kept++]=current; }
    for (size_t index=0;index<state->epsilon_count;++index) {
      if ((status=srl_xml_scalar_spend(work,1u))!=srl_ok) { return status; }
      const size_t target=state->epsilon[index];if (!visited[target]) { visited[target]=true;stack[pending++]=target; }
    }
  } *count=kept;return srl_ok;
}
/* Run the compiler-owned Thompson program over Unicode, with finite input and operation work. */
static inline srl_status srl_xml_scalar_match(const srl_xml_scalar_pattern* pattern, const char* text,
    srl_xml_scalar_work* work, bool* result) {
  const size_t size=strlen(text),states=pattern->state_count;
  if (size>srl_xml_scalar_pattern_bytes||states>SIZE_MAX/(3u*sizeof(size_t)+sizeof(bool))) { return srl_limit; }
  if (!states) { return srl_invalid; }
  void* allocation=malloc(states*(3u*sizeof(size_t)+sizeof(bool)));if (!allocation) { return srl_allocation; }
  size_t* active=(size_t*)allocation;size_t* next=active+states;size_t* stack=next+states;bool* visited=(bool*)(stack+states);
  active[0]=pattern->start;size_t count=1,offset=0;srl_status status=srl_xml_scalar_closure(pattern,active,&count,visited,stack,work);
  while (status==srl_ok&&offset<size) {
    const uint32_t value=srl_xml_scalar_code(text,&offset);size_t following=0;
    for (size_t index=0;index<count;++index) {
      if ((status=srl_xml_scalar_spend(work,1u))!=srl_ok) { break; }
      const srl_xml_scalar_state* state=&pattern->states[active[index]];if (!(state->flags&1u)) { continue; } bool matched=false;
      if (state->flags&4u) { matched=value!=10u&&value!=13u; }
      else {
        for (size_t range=0;range<state->range_count;++range) {
          if ((status=srl_xml_scalar_spend(work,1u))!=srl_ok) { break; }
          if (value>=state->ranges[range].first&&value<=state->ranges[range].last) { matched=true; }
        } if (state->flags&2u) { matched=!matched; }
      }
      if (matched) { next[following++]=state->next; }
    }
    if (status!=srl_ok) { break; } count=following;status=srl_xml_scalar_closure(pattern,next,&count,visited,stack,work);
    size_t* exchanged=active;active=next;next=exchanged;
  }
  *result=false;if (status==srl_ok) { for (size_t index=0;index<count;++index) { if (pattern->states[active[index]].flags&8u) { *result=true;break; } } }
  free(allocation);return status;
}
/* Compare builtin logical values; NaN is unordered for numeric bounds and equal for enumeration/fixed. */
static inline srl_status srl_xml_scalar_compare_kind(const char* kind, const char* left, const char* right,
    bool equal_nan, int* order) {
  if (srl_xml_scalar_integer(kind)||strcmp(kind,"decimal")==0) { *order=srl_xml_scalar_decimal_compare(left,right); }
  else if (strcmp(kind,"boolean")==0) { bool a=false,b=false;if (!srl_xml_boolean(left,&a)||!srl_xml_boolean(right,&b)) { return srl_invalid; } *order=(a>b)-(a<b); }
  else if (strcmp(kind,"float")==0||strcmp(kind,"double")==0) {
    double a=0,b=0;if (!srl_xml_float(left,strcmp(kind,"float")==0,&a)||!srl_xml_float(right,strcmp(kind,"float")==0,&b)) { return srl_invalid; }
    *order=isnan(a)||isnan(b)?(equal_nan&&isnan(a)&&isnan(b)?0:2):(a>b)-(a<b);
  } else { const int compared=strcmp(left,right);*order=(compared>0)-(compared<0); } return srl_ok;
}
/* Match list values itemwise rather than comparing their equivalent numeric/Boolean spellings as strings. */
static inline srl_status srl_xml_scalar_compare(const srl_xml_scalar_rule* rule, const char* left, const char* right,
    bool equal_nan, srl_xml_scalar_work* work, int* order) {
  if (!*rule->list_kind) { return srl_xml_scalar_compare_kind(rule->kind,left,right,equal_nan,order); }
  size_t left_count=0,right_count=0;srl_status status=srl_xml_scalar_count(left,work->max_items,&left_count);
  if (status==srl_ok) { status=srl_xml_scalar_count(right,work->max_items,&right_count); } if (status!=srl_ok) { return status; }
  *order=1;if (left_count!=right_count) { return srl_ok; }
  size_t a_offset=0,b_offset=0,a_begin=0,b_begin=0,a_size=0,b_size=0;
  while (srl_xml_scalar_token(left,&a_offset,&a_begin,&a_size)&&srl_xml_scalar_token(right,&b_offset,&b_begin,&b_size)) {
    if (a_size>SIZE_MAX-b_size||(status=srl_xml_scalar_spend(work,a_size+b_size))!=srl_ok) { return srl_limit; }
    srl_string a={0},b={0};status=srl_string_set(&a,left+a_begin,a_size);
    if (status==srl_ok) { status=srl_string_set(&b,right+b_begin,b_size); }
    if (status==srl_ok) { status=srl_xml_scalar_compare_kind(rule->list_kind,a.data,b.data,equal_nan,order); }
    srl_string_free(&a);srl_string_free(&b);if (status!=srl_ok||*order!=0) { return status; }
  } *order=0;return srl_ok;
}
/* Compare primitive type families independently of authored member names and bounded integer widths. */
static inline const char* srl_xml_scalar_value_family(const char* kind) {
  if (srl_xml_scalar_integer(kind)||!strcmp(kind,"decimal")) { return "decimal"; }
  return !strcmp(kind,"boolean")||!strcmp(kind,"float")||!strcmp(kind,"double")||!strcmp(kind,"anyURI")||!strcmp(kind,"QName") ? kind : "string";
}
/* Keep scalar and ordered-list identities distinct while admitting derived numeric item families. */
static inline bool srl_xml_scalar_same_family(const srl_xml_scalar_rule* first,const srl_xml_scalar_rule* second) {
  if (!!*first->list_kind!=!!*second->list_kind) { return false; }
  return !strcmp(srl_xml_scalar_value_family(*first->list_kind?first->list_kind:first->kind),srl_xml_scalar_value_family(*second->list_kind?second->list_kind:second->kind));
}
/* Compare an outer enumeration/fixed literal through the union's own ordered members. */
static inline srl_status srl_xml_scalar_union_equal(const srl_xml_scalar_rule* rule,const srl_xml_scalar_rule* selected,
    const char* text,const char* allowed,srl_xml_scalar_work* work,int* order);
/* Compare a prepared schema identity without reselecting its authored lexical spelling. */
static inline srl_status srl_xml_scalar_stored_equal(const srl_xml_scalar_rule*,const char*,const srl_xml_scalar_identity*,srl_xml_scalar_work*,bool*);
/* Validate one normalized candidate, retaining resource failures across independent union alternatives. */
static inline srl_status srl_xml_scalar_check_selected(const srl_xml_scalar_rule* rule, const char* raw, size_t size,
    srl_xml_scalar_work* work, srl_string* result,const srl_xml_scalar_rule** selected_leaf) {
  const srl_xml_scalar_rule* selected=rule;
  srl_status status=srl_xml_scalar_spend(work,size);if (status!=srl_ok) { return status; } srl_string text={0};
  status=srl_xml_scalar_normalize(raw,size,rule->member_count?"preserve":rule->whitespace,&text);if (status!=srl_ok) { return status; }
  if (!rule->member_count) { status=srl_xml_scalar_lexical(rule->kind,text.data);if (status!=srl_ok) { goto done; } }
  if (rule->member_count) {
    bool matched=false;
    for (size_t index=0;index<rule->member_count;++index) {
      srl_string alternative={0};status=srl_xml_scalar_check_selected(&rule->members[index],text.data,text.size,work,&alternative,&selected);
      if (status==srl_ok) { srl_string_free(&text);text=alternative;matched=true;break; } srl_string_free(&alternative);if (status==srl_limit||status==srl_allocation) { goto done; }
    } if (!matched) { status=srl_invalid;goto done; }
  }
  size_t count=0;
  if (*rule->list_kind) {
    status=srl_xml_scalar_count(text.data,work->max_items,&count);if (status!=srl_ok) { goto done; }
    size_t offset=0,begin=0,item_size=0;
    while (srl_xml_scalar_token(text.data,&offset,&begin,&item_size)) {
      srl_string item={0};status=srl_xml_scalar_spend(work,item_size);
      if (status==srl_ok) { status=srl_string_set(&item,text.data+begin,item_size); }
      if(status==srl_ok&&rule->item_rule){srl_string checked={0};status=srl_xml_scalar_check_selected(rule->item_rule,item.data,item.size,work,&checked,NULL);srl_string_free(&checked);}
      else if(status==srl_ok){status=srl_xml_scalar_lexical(rule->list_kind,item.data);}
      srl_string_free(&item);if (status!=srl_ok) { goto done; }
    }
  } else { size_t offset=0;while (offset<text.size) { srl_xml_scalar_code(text.data,&offset);++count; } }
  if ((rule->length!=SIZE_MAX&&count!=rule->length)||(rule->minimum!=SIZE_MAX&&count<rule->minimum)||
      (rule->maximum!=SIZE_MAX&&count>rule->maximum)) { status=srl_invalid;goto done; }
  for (size_t index=0;index<sizeof(rule->bounds)/sizeof(rule->bounds[0]);++index) {
    if (!*rule->bounds[index]) { continue; } int order=0;status=srl_xml_scalar_compare(rule,text.data,rule->bounds[index],false,work,&order);
    if (status!=srl_ok) { goto done; }
    if (order==2||((index==0u||index==2u)&&order<0)||((index==1u||index==3u)&&order>0)||(index>=2u&&order==0)) { status=srl_invalid;goto done; }
  }
  if(rule->identity_count){
    bool matched=false;for(size_t index=0;status==srl_ok&&!matched&&index<rule->identity_count;++index){
      status=srl_xml_scalar_stored_equal(selected,text.data,&rule->identities[index],work,&matched);
    }if(status!=srl_ok){goto done;}if(!matched){status=srl_invalid;goto done;}
  }
  if(rule->fixed_identity){bool matched=false;status=srl_xml_scalar_stored_equal(selected,text.data,rule->fixed_identity,work,&matched);if(status!=srl_ok){goto done;}if(!matched){status=srl_invalid;goto done;}}
  if (rule->enumeration_count) {
    bool matched=false;
    for (size_t index=0;index<rule->enumeration_count;++index) {
      srl_string expected={0};int order=0;if (rule->member_count) { status=srl_xml_scalar_union_equal(rule,selected,text.data,rule->enumeration[index],work,&order); } else {
        status=srl_xml_scalar_normalize(rule->enumeration[index],strlen(rule->enumeration[index]),rule->whitespace,&expected);
        if (status==srl_ok) { status=srl_xml_scalar_compare(rule,text.data,expected.data,true,work,&order); }
      } srl_string_free(&expected);
      if (status!=srl_ok) { goto done; } if (!order) { matched=true;break; }
    } if (!matched) { status=srl_invalid;goto done; }
  }
  if (rule->fixed) {
    srl_string expected={0};int order=0;if (rule->member_count) { status=srl_xml_scalar_union_equal(rule,selected,text.data,rule->fixed,work,&order); } else {
      status=srl_xml_scalar_normalize(rule->fixed,strlen(rule->fixed),rule->whitespace,&expected);
      if (status==srl_ok) { status=srl_xml_scalar_compare(rule,text.data,expected.data,true,work,&order); }
    } srl_string_free(&expected);
    if (status!=srl_ok) { goto done; } if (order) { status=srl_invalid;goto done; }
  }
  if (rule->finite) { double number=0;if (!srl_xml_float(text.data,strcmp(rule->kind,"float")==0,&number)||!isfinite(number)) { status=srl_invalid;goto done; } }
  for (size_t group=0;group<rule->group_count;++group) {
    bool matched=false;
    for (size_t index=0;index<rule->groups[group].count;++index) {
      status=srl_xml_scalar_match(&rule->groups[group].patterns[index],text.data,work,&matched);
      if (status!=srl_ok) { goto done; } if (matched) { break; }
    } if (!matched) { status=srl_invalid;goto done; }
  }
done:
  if (status==srl_ok) { if(selected_leaf){*selected_leaf=selected;}srl_string_free(result);*result=text; } else { srl_string_free(&text); } return status;
}
/* Compare selected item identities in order, preserving independent union families per token. */
static inline srl_status srl_xml_scalar_stored_equal(const srl_xml_scalar_rule* selected,const char* text,
    const srl_xml_scalar_identity* identity,srl_xml_scalar_work* work,bool* equal) {
  *equal=false;srl_status status=srl_xml_scalar_spend(work,strlen(text));if(status!=srl_ok){return status;}
  srl_xml_scalar_rule expected={0};expected.kind=identity->kind;expected.list_kind=identity->list_kind;
  if(!srl_xml_scalar_same_family(selected,&expected)){return srl_ok;}
  if(*selected->list_kind){
    size_t count=0;status=srl_xml_scalar_count(text,work->max_items,&count);if(status!=srl_ok||count!=identity->item_count){return status;}
    size_t offset=0,begin=0,size=0,index=0;bool matched=true;
    srl_xml_scalar_rule builtin={0};builtin.kind=selected->list_kind;builtin.whitespace="collapse";builtin.list_kind="";
    builtin.length=builtin.minimum=builtin.maximum=SIZE_MAX;for(size_t bound=0;bound<sizeof(builtin.bounds)/sizeof(builtin.bounds[0]);++bound){builtin.bounds[bound]="";}
    while(status==srl_ok&&matched&&srl_xml_scalar_token(text,&offset,&begin,&size)){
      srl_string token={0},checked={0};const srl_xml_scalar_rule* item=&builtin;
      status=srl_string_set(&token,text+begin,size);
      if(status==srl_ok&&selected->item_rule){status=srl_xml_scalar_check_selected(selected->item_rule,token.data,token.size,work,&checked,&item);}
      if(status==srl_ok){status=srl_xml_scalar_stored_equal(item,selected->item_rule?checked.data:token.data,&identity->items[index++],work,&matched);}
      srl_string_free(&token);srl_string_free(&checked);
    }*equal=status==srl_ok&&matched;return status;
  }
  int order=1;status=srl_xml_scalar_compare(selected,text,identity->lexical,true,work,&order);*equal=status==srl_ok&&!order;return status;
})cscalar"
R"cscalar(
/* Resolve schema literals without applying the outer enumeration recursively. */
static inline srl_status srl_xml_scalar_union_equal(const srl_xml_scalar_rule* rule,const srl_xml_scalar_rule* selected,
    const char* text,const char* allowed,srl_xml_scalar_work* work,int* order) {
  *order=1;
  for(size_t index=0;index<rule->member_count;++index) {
    srl_string expected={0};const srl_xml_scalar_rule* leaf=NULL;
    srl_status status=srl_xml_scalar_check_selected(&rule->members[index],allowed,strlen(allowed),work,&expected,&leaf);
    if(status==srl_ok) { if(srl_xml_scalar_same_family(selected,leaf)){status=srl_xml_scalar_compare(selected,text,expected.data,true,work,order);}srl_string_free(&expected);return status; }
    srl_string_free(&expected);if(status==srl_limit||status==srl_allocation) {return status;}
  }return srl_ok;
}
/* Preserve the original union spelling so a later encode cannot select an earlier different member. */
static inline srl_status srl_xml_scalar_check_work(const srl_xml_scalar_rule* rule,const char* raw,size_t size,
    srl_xml_scalar_work* work,srl_string* result) {
  /* Namespace-aware union selection is performed by the owning XML companion before publication. */
  if (rule->namespace_dependent) {
    srl_status status=srl_xml_scalar_spend(work,size);
    return status==srl_ok ? srl_xml_scalar_normalize(raw,size,rule->member_count?"preserve":rule->whitespace,result) : status;
  }
  srl_string checked={0};srl_status status=srl_xml_scalar_check_selected(rule,raw,size,work,&checked,NULL);
  if(status==srl_ok&&rule->member_count) {status=srl_string_set(result,raw,size);srl_string_free(&checked);}else if(status==srl_ok){srl_string_free(result);*result=checked;}return status;
}
/* Start independent scalar work only after document admission has bounded the raw text. */
static inline srl_status srl_xml_scalar_check(const srl_xml_scalar_rule* rule, const char* raw, size_t size,
    size_t maximum_items, srl_string* result) {
  srl_xml_scalar_work work={srl_xml_scalar_max_work,maximum_items};return srl_xml_scalar_check_work(rule,raw,size,&work,result);
}
/* Preserve an explicit normalized empty collapse value instead of reactivating a different element default. */
static inline srl_status srl_xml_scalar_output(const srl_xml_scalar_rule* rule, const char* raw, size_t size,
    const char* fallback, size_t maximum_items, srl_string* result) {
  srl_status status=srl_xml_scalar_check(rule,raw,size,maximum_items,result);if (status!=srl_ok||result->size||!fallback) { return status; }
  srl_string expected={0};status=srl_xml_scalar_normalize(fallback,strlen(fallback),rule->whitespace,&expected);
  if (status==srl_ok&&expected.size) { status=strcmp(rule->whitespace,"collapse")==0?srl_string_set(result," ",1u):srl_invalid; }
  srl_string_free(&expected);return status;
}
/* Append one native list item, checking separators, item spelling and text admission before reallocation. */
static inline srl_status srl_xml_scalar_append(srl_buffer* joined, const char* text, size_t size, size_t maximum) {
  if (!size) { return srl_invalid; } for (size_t index=0;index<size;++index) { if (srl_xml_scalar_space((unsigned char)text[index])) { return srl_invalid; } }
  const size_t separator=joined->size?1u:0u;if (joined->size>maximum||separator>maximum-joined->size||size>maximum-joined->size-separator) { return srl_limit; }
  if (size>SIZE_MAX-joined->size-separator) { return srl_limit; } const size_t count=joined->size+separator+size;
  if (!srl_reserve(joined,separator+size)) { return srl_allocation; }
  if (separator) { joined->data[joined->size++]=' '; } memcpy(joined->data+joined->size,text,size);joined->size=count;return srl_ok;
}
)cscalar"
