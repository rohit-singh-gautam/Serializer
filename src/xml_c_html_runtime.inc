// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"chtml(
/* Independent ordinary HTML admission; every size is a byte/event count, and zero forbids that resource. */
typedef struct srl_html_limits {
  size_t max_input_bytes, max_output_bytes, max_events, max_depth, max_attributes_per_element, max_text_bytes;
} srl_html_limits;
/* Semantic events have paired void starts/ends and no XML namespace or xsi metadata. */
typedef enum srl_html_event_kind {
  srl_html_start_fragment, srl_html_end_fragment, srl_html_start_element, srl_html_end_element, srl_html_text
} srl_html_event_kind;
/* Parsed names borrow immutable approved runtime spellings; values/text and arrays are owned by the returned trace. */
typedef struct srl_html_attribute { const char* name; srl_string value; bool boolean; } srl_html_attribute;
typedef struct srl_html_event {
  srl_html_event_kind kind; const char* name; srl_html_attribute* attributes; size_t attribute_count; srl_string text;
} srl_html_event;
typedef struct srl_html_events { srl_html_event* data; size_t size; } srl_html_events;
/* Derive one conservative fragment policy independently of native and XML document limits. */
static inline srl_html_limits srl_html_default_limits(void) {
  const srl_html_limits limits = {8388608u,16777216u,200000u,128u,256u,8388608u}; return limits;
}
/* Describe the exact subset without claiming general HTML5 parsing or URL sanitization. */
static inline const char* srl_html_supported_subset(void) {
  return "UTF-8 HTML fragments; explicit balanced approved flow/phrasing/list tags; br/hr/img void tags; ASCII case-insensitive names; quoted/unquoted and hidden Boolean attributes; amp/lt/gt/quot/apos/nbsp and numeric references; comments omitted; no doctype, implied closes, raw text, foreign content or browser recovery";
}
/* Free one initialized owned parsed event; names remain immutable borrowed metadata. */
static inline void srl_html_event_free(srl_html_event* event) {
  for (size_t index=0; index<event->attribute_count; ++index) { srl_string_free(&event->attributes[index].value); }
  free(event->attributes); srl_string_free(&event->text); memset(event,0,sizeof(*event));
}
/* Free an initialized trace returned by parse_fragment; zero initialization and repeated cleanup are valid. */
static inline void srl_html_events_free(srl_html_events* events) {
  for (size_t index=0; index<events->size; ++index) { srl_html_event_free(&events->data[index]); }
  free(events->data); memset(events,0,sizeof(*events));
}
/* Compare optional semantic payload names without dereferencing a missing empty-field pointer. */
static inline bool srl_html_empty_name(const char* name) { return !name || !*name; }
/* Resolve only approved lowercase names to immutable metadata, never an arbitrary tag vocabulary. */
static inline const char* srl_html_tag(const char* name) {
  static const char* const names[] = {"div","p","span","b","i","strong","em","a","ul","ol","li","section","article","h1","h2","h3","h4","h5","h6","br","hr","img","small","s","u","code","blockquote","nav"};
  if (!name) { return NULL; }
  for (size_t index=0; index<sizeof(names)/sizeof(names[0]); ++index) { if (strcmp(name,names[index])==0) { return names[index]; } }
  return NULL;
}
/* Identify the approved void subset; its semantic event trace still includes matching end events. */
static inline bool srl_html_void(const char* name) { return name && (strcmp(name,"br")==0 || strcmp(name,"hr")==0 || strcmp(name,"img")==0); }
/* Identify phrasing elements whose accepted children cannot require browser rearrangement. */
static inline bool srl_html_phrasing(const char* name) {
  static const char* const names[] = {"span","b","i","strong","em","a","br","img","small","s","u","code"};
  for (size_t index=0; index<sizeof(names)/sizeof(names[0]); ++index) { if (strcmp(name,names[index])==0) { return true; } } return false;
}
/* Recognize only fixed HTML ASCII whitespace, including form feed. */
static inline bool srl_html_space(uint8_t value) { return value==' ' || value=='\t' || value=='\n' || value=='\r' || value==12; }
/* Reject unsupported semantic controls while allowing ordinary HTML form feed and all valid Unicode scalars. */
static inline bool srl_html_scalar(uint32_t code) {
  return code && code<=0x10ffffu && !(code>=0xd800u && code<=0xdfffu) && !(code>=127u && code<=159u) &&
      (code>=32u || code==9u || code==10u || code==12u || code==13u);
}
/* Validate strict UTF8 and HTML controls without imposing XML-specific character rules. */
static inline bool srl_html_valid_text(const uint8_t* data, size_t size) {
  if (!srl_utf8(data,size)) { return false; }
  for (size_t index=0; index<size; ++index) {
    const uint8_t byte=data[index];
    if ((byte<128u && !srl_html_scalar(byte)) || (byte==0xc2u && index+1<size && data[index+1]>=0x80u && data[index+1]<=0x9fu)) { return false; }
  } return true;
}
/* Refuse relationships that independent HTML parsers would implicitly close or rearrange. */
static inline bool srl_html_child(const char* const* open, size_t depth, const char* child) {
  if (!srl_html_tag(child)) { return false; }
  const char* parent=depth ? open[depth-1] : NULL;
  if (strcmp(child,"li")==0 && (!parent || (strcmp(parent,"ul")!=0 && strcmp(parent,"ol")!=0))) { return false; }
  if (parent) {
    const bool list=strcmp(parent,"ul")==0 || strcmp(parent,"ol")==0;
    const bool phrasing=srl_html_phrasing(parent) || strcmp(parent,"p")==0 || (parent[0]=='h' && parent[1]>='1' && parent[1]<='6' && !parent[2]);
    if (srl_html_void(parent) || (list && strcmp(child,"li")!=0) || (phrasing && !srl_html_phrasing(child))) { return false; }
  }
  if (strcmp(child,"a")==0) { for (size_t index=0; index<depth; ++index) { if (strcmp(open[index],"a")==0) { return false; } } }
  return true;
}
/* Resolve an approved attribute spelling; no event handlers, foreign attributes or sanitizer claims are implied. */
static inline const char* srl_html_attribute_name(const char* tag, const char* key) {
  static const char* const ordinary[] = {"id","class","title","lang","dir","hidden"};
  if (!key) { return NULL; }
  for (size_t index=0; index<sizeof(ordinary)/sizeof(ordinary[0]); ++index) { if (strcmp(key,ordinary[index])==0) { return ordinary[index]; } }
  if (strcmp(tag,"a")==0) { if (strcmp(key,"href")==0) { return "href"; } if (strcmp(key,"rel")==0) { return "rel"; } }
  if (strcmp(tag,"img")==0) {
    static const char* const image[] = {"src","alt","width","height"};
    for (size_t index=0; index<sizeof(image)/sizeof(image[0]); ++index) { if (strcmp(key,image[index])==0) { return image[index]; } }
  } return NULL;
}
/* Retain only active ancestor names; grow after checking depth and native multiplication bounds. */
static inline srl_status srl_html_push(const char*** open, size_t* depth, size_t* capacity, const char* name, size_t maximum) {
  if (*depth>=maximum) { return srl_limit; }
  if (*depth==*capacity) {
    size_t next=*capacity ? *capacity : 8u;
    if (*capacity) { next=*capacity>SIZE_MAX/2 ? maximum : *capacity*2; }
    if (next>maximum) { next=maximum; }
    if (next<=*depth || next>SIZE_MAX/sizeof(**open)) { return srl_limit; }
    void* memory=realloc((void*)*open,next*sizeof(**open)); if (!memory) { return srl_allocation; }
    *open=(const char**)memory; *capacity=next;
  }
  (*open)[(*depth)++]=name; return srl_ok;
}
/* Admit output bytes before growing the unpublished candidate buffer. */
static inline void srl_html_put(srl_writer* output, const char* data, size_t size, size_t maximum) {
  if (output->status!=srl_ok) { return; }
  if (output->buffer.size>maximum || size>maximum-output->buffer.size) { output->status=srl_limit; return; }
  srl_put(output,data,size);
}
/* Append one fixed spelling without repeating its caller-side length arithmetic. */
static inline void srl_html_put_text(srl_writer* output, const char* value, size_t maximum) { srl_html_put(output,value,strlen(value),maximum); }
/* Escape semantic text while preserving referenced CR and HTML-only form-feed scalars exactly. */
static inline void srl_html_escape(srl_writer* output, const srl_string* text, bool attribute, size_t maximum) {
  if (!srl_html_valid_text((const uint8_t*)text->data,text->size)) { output->status=srl_invalid; return; }
  size_t first=0;
  for (size_t index=0; index<text->size && output->status==srl_ok; ++index) {
    const char* replacement=NULL;
    switch (text->data[index]) { case '&': replacement="&amp;"; break; case '<': replacement="&lt;"; break; case '>': replacement="&gt;"; break; case '\r': replacement="&#13;"; break; case '"': if (attribute) { replacement="&quot;"; } break; default: break; }
    if (replacement) { srl_html_put(output,text->data+first,index-first,maximum); srl_html_put_text(output,replacement,maximum); first=index+1; }
  }
  if (first<text->size) { srl_html_put(output,text->data+first,text->size-first,maximum); }
}
/* Validate the complete borrowed trace before replacing an initialized output buffer; failures leave it unchanged. */
static inline srl_status srl_html_write_fragment(srl_buffer* result, const srl_html_events* events, const srl_html_limits* policy) {
  const srl_html_limits limits=policy ? *policy : srl_html_default_limits();
  if (!result || !events || events->size<2 || !events->data || events->size>limits.max_events || events->data[0].kind!=srl_html_start_fragment || events->data[events->size-1].kind!=srl_html_end_fragment) { return srl_invalid; }
  srl_writer output={0}; const char** open=NULL; size_t depth=0,capacity=0,text_bytes=0;
  for (size_t index=0; index<events->size && output.status==srl_ok; ++index) {
    const srl_html_event* event=&events->data[index];
    switch (event->kind) {
      case srl_html_start_fragment: case srl_html_end_fragment:
        if ((index!=0 && index!=events->size-1) || depth || !srl_html_empty_name(event->name) || event->attribute_count || event->text.size) { output.status=srl_invalid; } break;
      case srl_html_start_element: {
        if (!srl_html_child(open,depth,event->name) || event->text.size || event->attribute_count>limits.max_attributes_per_element || (event->attribute_count && !event->attributes)) { output.status=srl_invalid; break; }
        output.status=srl_html_push(&open,&depth,&capacity,event->name,limits.max_depth); if (output.status!=srl_ok) { break; }
        srl_html_put_text(&output,"<",limits.max_output_bytes); srl_html_put_text(&output,event->name,limits.max_output_bytes);
        for (size_t attribute=0; attribute<event->attribute_count && output.status==srl_ok; ++attribute) {
          const srl_html_attribute* value=&event->attributes[attribute];
          if (!srl_html_attribute_name(event->name,value->name) || value->boolean!=(strcmp(value->name,"hidden")==0) || (value->boolean && value->value.size)) { output.status=srl_invalid; break; }
          for (size_t previous=0; previous<attribute; ++previous) { if (strcmp(value->name,event->attributes[previous].name)==0) { output.status=srl_invalid; break; } }
          if (!srl_html_valid_text((const uint8_t*)value->value.data,value->value.size)) { output.status=srl_invalid; break; }
          srl_html_put_text(&output," ",limits.max_output_bytes); srl_html_put_text(&output,value->name,limits.max_output_bytes);
          if (!value->boolean) { srl_html_put_text(&output,"=\"",limits.max_output_bytes); srl_html_escape(&output,&value->value,true,limits.max_output_bytes); srl_html_put_text(&output,"\"",limits.max_output_bytes); }
        }
        srl_html_put_text(&output,">",limits.max_output_bytes); break;
      }
      case srl_html_end_element:
        if (!depth || !event->name || strcmp(open[depth-1],event->name)!=0 || event->attribute_count || event->text.size) { output.status=srl_invalid; break; }
        if (!srl_html_void(event->name)) { srl_html_put_text(&output,"</",limits.max_output_bytes); srl_html_put_text(&output,event->name,limits.max_output_bytes); srl_html_put_text(&output,">",limits.max_output_bytes); }
        --depth; break;
      case srl_html_text:
        if (!srl_html_empty_name(event->name) || event->attribute_count || (depth && srl_html_void(open[depth-1])) || event->text.size>limits.max_text_bytes-text_bytes) { output.status=srl_invalid; break; }
        if (depth && (strcmp(open[depth-1],"ul")==0 || strcmp(open[depth-1],"ol")==0)) {
          for (size_t item=0; item<event->text.size; ++item) { if (!event->text.data || !srl_html_space((uint8_t)event->text.data[item])) { output.status=srl_invalid; break; } }
        }
        text_bytes+=event->text.size; srl_html_escape(&output,&event->text,false,limits.max_output_bytes); break;
      default: output.status=srl_invalid; break;
    }
  }
  if (depth && output.status==srl_ok) { output.status=srl_invalid; } free((void*)open);
  if (output.status!=srl_ok) { srl_buffer_free(&output.buffer); return output.status; }
  srl_buffer_free(result); *result=output.buffer; return srl_ok;
}
)chtml"
R"chtml(
/* Decode terminated approved references in two passes, checking semantic size before any owned string allocation. */
static inline srl_status srl_html_decode_pass(const uint8_t* source, size_t size, char* output, size_t* result) {
  size_t used=0;
  for (size_t position=0; position<size;) {
    uint32_t code=source[position++];
    if (code=='\r') { if (position<size && source[position]=='\n') { ++position; } code='\n'; }
    else if (code=='&') {
      const size_t begin=position; size_t end=begin;
      while (end<size && end-begin<12 && source[end]!=';') { ++end; }
      if (end==size || end-begin>=12 || source[end]!=';') { return srl_invalid; }
      const size_t count=end-begin; const uint8_t* text=source+begin;
      if (count==3 && memcmp(text,"amp",3)==0) { code='&'; }
      else if (count==2 && memcmp(text,"lt",2)==0) { code='<'; }
      else if (count==2 && memcmp(text,"gt",2)==0) { code='>'; }
      else if (count==4 && memcmp(text,"quot",4)==0) { code='"'; }
      else if (count==4 && memcmp(text,"apos",4)==0) { code='\''; }
      else if (count==4 && memcmp(text,"nbsp",4)==0) { code=160u; }
      else {
        if (!count || text[0]!='#') { return srl_invalid; }
        size_t digit=1; uint32_t radix=10u;
        if (digit<count && (text[digit]=='x' || text[digit]=='X')) { ++digit; radix=16u; }
        if (digit==count) { return srl_invalid; } code=0;
        for (; digit<count; ++digit) {
          const uint8_t byte=text[digit]; uint32_t value;
          if (byte>='0' && byte<='9') { value=(uint32_t)(byte-'0'); }
          else if (radix==16u && byte>='a' && byte<='f') { value=(uint32_t)(byte-'a')+10u; }
          else if (radix==16u && byte>='A' && byte<='F') { value=(uint32_t)(byte-'A')+10u; }
          else { return srl_invalid; }
          if (code>(0x10ffffu-value)/radix) { return srl_invalid; } code=code*radix+value;
        }
      }
      if (!srl_html_scalar(code)) { return srl_invalid; } position=end+1;
      uint8_t bytes[4]; size_t width;
      if (code<128u) { bytes[0]=(uint8_t)code; width=1; }
      else if (code<2048u) { bytes[0]=(uint8_t)(0xc0u|(code>>6)); bytes[1]=(uint8_t)(0x80u|(code&63u)); width=2; }
      else if (code<65536u) { bytes[0]=(uint8_t)(0xe0u|(code>>12)); bytes[1]=(uint8_t)(0x80u|((code>>6)&63u)); bytes[2]=(uint8_t)(0x80u|(code&63u)); width=3; }
      else { bytes[0]=(uint8_t)(0xf0u|(code>>18)); bytes[1]=(uint8_t)(0x80u|((code>>12)&63u)); bytes[2]=(uint8_t)(0x80u|((code>>6)&63u)); bytes[3]=(uint8_t)(0x80u|(code&63u)); width=4; }
      if (width>size-used) { return srl_invalid; } if (output) { memcpy(output+used,bytes,width); } used+=width; continue;
    }
    if (used==size) { return srl_invalid; } if (output) { output[used]=(char)code; } ++used;
  }
  *result=used; return srl_ok;
}
/* Replace one initialized string only after complete lexical validation, size admission and successful allocation. */
static inline srl_status srl_html_decode(srl_string* result, const uint8_t* source, size_t size, size_t maximum) {
  size_t count=0; srl_status status=srl_html_decode_pass(source,size,NULL,&count);
  if (status!=srl_ok) { return status; } if (count>maximum || count==SIZE_MAX) { return srl_limit; }
  char* text=(char*)malloc(count+1); if (!text) { return srl_allocation; }
  size_t actual=0; status=srl_html_decode_pass(source,size,text,&actual);
  if (status!=srl_ok || actual!=count) { free(text); return status==srl_ok ? srl_invalid : status; }
  text[count]='\0'; srl_string_free(result); result->data=text; result->size=count; return srl_ok;
}
/* Read a bounded ASCII syntax name and normalize only ASCII case, independently of the process locale. */
static inline srl_status srl_html_name(const uint8_t* source, size_t size, size_t* position, char* result, size_t capacity) {
  const size_t start=*position;
  while (*position<size) {
    const uint8_t byte=source[*position];
    if (!((byte>='a' && byte<='z') || (byte>='A' && byte<='Z') || (byte>='0' && byte<='9') || byte=='-')) { break; }
    ++*position;
  }
  if (*position==start || *position-start>=capacity) { return srl_invalid; }
  for (size_t index=start; index<*position; ++index) { const uint8_t byte=source[index]; result[index-start]=(char)(byte>='A' && byte<='Z' ? byte+('a'-'A') : byte); }
  result[*position-start]='\0'; return srl_ok;
}
/* Advance only through HTML ASCII whitespace without allocating unconsumed source suffixes. */
static inline size_t srl_html_skip(const uint8_t* source, size_t size, size_t position) { while (position<size && srl_html_space(source[position])) { ++position; } return position; }
/* Append one owned event only after event/text admission and overflow-checked vector growth. */
static inline srl_status srl_html_add(srl_html_events* events, size_t* capacity, srl_html_event* event, size_t* text_bytes, const srl_html_limits* limits) {
  if (events->size>=limits->max_events) { return srl_limit; }
  if (event->kind==srl_html_text && event->text.size>limits->max_text_bytes-*text_bytes) { return srl_limit; }
  if (events->size==*capacity) {
    size_t next=*capacity ? (*capacity>SIZE_MAX/2 ? limits->max_events : *capacity*2) : 8u;
    if (next>limits->max_events) { next=limits->max_events; }
    if (next<=events->size || next>SIZE_MAX/sizeof(*events->data)) { return srl_limit; }
    void* memory=realloc(events->data,next*sizeof(*events->data)); if (!memory) { return srl_allocation; }
    events->data=(srl_html_event*)memory; *capacity=next;
  }
  if (event->kind==srl_html_text) { *text_bytes+=event->text.size; }
  events->data[events->size++]=*event; memset(event,0,sizeof(*event)); return srl_ok;
}
/* Parse fatal UTF8 with explicit nesting; replace an initialized trace only after complete validated EOF. */
static inline srl_status srl_html_parse_fragment(srl_html_events* result, const uint8_t* source, size_t size, const srl_html_limits* policy) {
  const srl_html_limits limits=policy ? *policy : srl_html_default_limits();
  if (!result) { return srl_invalid; } if (size>limits.max_input_bytes) { return srl_limit; }
  if (!srl_html_valid_text(source,size)) { return srl_invalid; }
  srl_html_events candidate={0}; srl_html_event event={0}; const char** open=NULL;
  size_t depth=0,open_capacity=0,capacity=0,position=0,text_bytes=0; srl_status status;
  event.kind=srl_html_start_fragment; status=srl_html_add(&candidate,&capacity,&event,&text_bytes,&limits);
  while (position<size && status==srl_ok) {
    if (source[position]!='<') {
      size_t end=position; while (end<size && source[end]!='<') { ++end; }
      event.kind=srl_html_text; status=srl_html_decode(&event.text,source+position,end-position,limits.max_text_bytes-text_bytes);
      if (status==srl_ok) { status=srl_html_add(&candidate,&capacity,&event,&text_bytes,&limits); } position=end; continue;
    }
    if (size-position>=4 && memcmp(source+position,"<!--",4)==0) {
      size_t end=position+4; while (end<size && !(size-end>=3 && memcmp(source+end,"-->",3)==0)) { ++end; }
      if (end==size) { status=srl_invalid; break; }
      for (size_t check=position+4; check<end; ++check) { if (end-check>=2 && source[check]=='-' && source[check+1]=='-') { status=srl_invalid; break; } }
      position=end+3; continue;
    }
    ++position; const bool closing=position<size && source[position]=='/'; if (closing) { ++position; }
    char spelling[sizeof("blockquote")]; status=srl_html_name(source,size,&position,spelling,sizeof(spelling)); if (status!=srl_ok) { break; }
    const char* tag=srl_html_tag(spelling); if (!tag) { status=srl_invalid; break; }
    if (closing) {
      position=srl_html_skip(source,size,position);
      if (position==size || source[position]!='>' || srl_html_void(tag) || !depth || strcmp(open[depth-1],tag)!=0) { status=srl_invalid; break; }
      ++position; --depth; event.kind=srl_html_end_element; event.name=tag;
      status=srl_html_add(&candidate,&capacity,&event,&text_bytes,&limits); continue;
    }
    if (!srl_html_child(open,depth,tag)) { status=srl_invalid; break; }
    if (depth>=limits.max_depth) { status=srl_limit; break; }
    event.kind=srl_html_start_element; event.name=tag; size_t attribute_capacity=0; bool self_closing=false;
    while (status==srl_ok) {
      const size_t separator=position; position=srl_html_skip(source,size,position);
      if (position==size) { status=srl_invalid; break; }
      if (source[position]=='>') { ++position; break; }
      if (size-position>=2 && source[position]=='/' && source[position+1]=='>') { self_closing=true; position+=2; break; }
      if (separator==position) { status=srl_invalid; break; }
      if (event.attribute_count>=limits.max_attributes_per_element) { status=srl_limit; break; }
      char key_spelling[sizeof("blockquote")]; status=srl_html_name(source,size,&position,key_spelling,sizeof(key_spelling)); if (status!=srl_ok) { break; }
      const char* key=srl_html_attribute_name(tag,key_spelling); if (!key) { status=srl_invalid; break; }
      for (size_t previous=0; previous<event.attribute_count; ++previous) { if (strcmp(event.attributes[previous].name,key)==0) { status=srl_invalid; break; } }
      if (status!=srl_ok) { break; }
      if (event.attribute_count==attribute_capacity) {
        size_t next=attribute_capacity ? (attribute_capacity>SIZE_MAX/2 ? limits.max_attributes_per_element : attribute_capacity*2) : 4u;
        if (next>limits.max_attributes_per_element) { next=limits.max_attributes_per_element; }
        if (next<=event.attribute_count || next>SIZE_MAX/sizeof(*event.attributes)) { status=srl_limit; break; }
        void* memory=realloc(event.attributes,next*sizeof(*event.attributes)); if (!memory) { status=srl_allocation; break; }
        event.attributes=(srl_html_attribute*)memory; attribute_capacity=next;
      }
      srl_html_attribute* attribute=&event.attributes[event.attribute_count++]; memset(attribute,0,sizeof(*attribute)); attribute->name=key; attribute->boolean=strcmp(key,"hidden")==0;
      const size_t key_end=position; position=srl_html_skip(source,size,position); const bool assigned=position<size && source[position]=='=';
      if (assigned) {
        position=srl_html_skip(source,size,position+1); if (position==size) { status=srl_invalid; break; }
        const uint8_t quote=source[position]=='\'' || source[position]=='"' ? source[position++] : 0;
        const size_t start=position;
        if (quote) { while (position<size && source[position]!=quote) { ++position; } if (position==size) { status=srl_invalid; break; } }
        else {
          while (position<size && !srl_html_space(source[position]) && source[position]!='>') {
            const uint8_t byte=source[position]; if (byte=='"' || byte=='\'' || byte=='<' || byte=='=' || byte=='`') { status=srl_invalid; break; } ++position;
          }
          if (position==start) { status=srl_invalid; } if (status!=srl_ok) { break; }
        }
        status=srl_html_decode(&attribute->value,source+start,position-start,limits.max_input_bytes); if (status!=srl_ok) { break; } if (quote) { ++position; }
      } else { position=key_end; }
      if (attribute->boolean) {
        if (assigned && attribute->value.size) {
          const char* value=attribute->value.data;
          if (attribute->value.size!=sizeof("hidden")-1) { status=srl_invalid; break; }
          for (size_t index=0; index<sizeof("hidden")-1; ++index) {
            const uint8_t byte=(uint8_t)value[index]; if ((char)(byte>='A' && byte<='Z' ? byte+('a'-'A') : byte)!="hidden"[index]) { status=srl_invalid; break; }
          }
          if (status!=srl_ok) { break; }
        }
        srl_string_free(&attribute->value);
      } else if (!assigned) { status=srl_invalid; break; }
    }
    if (status!=srl_ok) { break; }
    if (self_closing && !srl_html_void(tag)) { status=srl_invalid; break; }
    status=srl_html_add(&candidate,&capacity,&event,&text_bytes,&limits); if (status!=srl_ok) { break; }
    if (srl_html_void(tag)) { event.kind=srl_html_end_element; event.name=tag; status=srl_html_add(&candidate,&capacity,&event,&text_bytes,&limits); }
    else { status=srl_html_push(&open,&depth,&open_capacity,tag,limits.max_depth); }
  }
  if (status==srl_ok && depth) { status=srl_invalid; }
  if (status==srl_ok) { event.kind=srl_html_end_fragment; status=srl_html_add(&candidate,&capacity,&event,&text_bytes,&limits); }
  if (status==srl_ok) { srl_buffer validation={0}; status=srl_html_write_fragment(&validation,&candidate,&limits); srl_buffer_free(&validation); }
  free((void*)open); srl_html_event_free(&event);
  if (status!=srl_ok) { srl_html_events_free(&candidate); return status; }
  srl_html_events_free(result); *result=candidate; return srl_ok;
}
)chtml"
