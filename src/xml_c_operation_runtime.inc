// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"coperation(
#ifndef SRL_XML_OPERATION_INCLUDED
#define SRL_XML_OPERATION_INCLUDED
/* Borrowed callbacks run synchronously; separate operations never share mutable state. */
typedef enum srl_xml_operation_resource {
  srl_xml_operation_work, srl_xml_operation_allocation, srl_xml_operation_nodes,
  srl_xml_operation_attributes, srl_xml_operation_scalar_bytes, srl_xml_operation_references
} srl_xml_operation_resource;
typedef enum srl_xml_operation_failure {
  srl_xml_operation_none, srl_xml_operation_resource_limit,
  srl_xml_operation_allocation_failure, srl_xml_operation_invalid
} srl_xml_operation_failure;
typedef int (*srl_xml_operation_consume_fn)(void*,srl_xml_operation_resource,size_t,const char*);
typedef int (*srl_xml_operation_poll_fn)(void*,const char*);
typedef int (*srl_xml_operation_preserve_fn)(void*,size_t,const char*);
typedef struct srl_xml_map_limits srl_xml_map_limits;
typedef struct srl_xml_mixed_limits srl_xml_mixed_limits;
typedef struct srl_xml_digest_limits srl_xml_digest_limits;
typedef struct srl_xml_identity_limits srl_xml_identity_limits;
/* Independent policies are borrowed for the synchronous call; existing policy aggregates stay unchanged. */
typedef struct srl_xml_operation_policies {
  const srl_xml_map_limits* map;
  const srl_xml_mixed_limits* mixed;
  const srl_xml_digest_limits* digest;
  const srl_xml_identity_limits* identity;
} srl_xml_operation_policies;
/* Immutable package requirements authorize only root transport metadata during this operation. */
typedef struct srl_xml_package_contract {
  bool requires_type; const char* const* vocabularies; size_t vocabulary_count;
} srl_xml_package_contract;
typedef struct srl_xml_operation {
  srl_xml_operation_consume_fn consume;
  srl_xml_operation_poll_fn poll;
  void* userdata;
  int caller_status;
  srl_xml_operation_failure failure;
  const char* field;
  const srl_xml_operation_policies* policies;
  const srl_xml_package_contract* package_contract;
  srl_xml_operation_preserve_fn preserve;
} srl_xml_operation;
typedef struct srl_xml_validation srl_xml_validation;
typedef struct srl_xml_operation_codec {
  srl_status (*encode)(const void*,srl_buffer*,const srl_limits*,const srl_xml_validation*,srl_xml_operation*);
  srl_status (*decode)(void*,const uint8_t*,size_t,const srl_limits*,const srl_xml_validation*,srl_xml_operation*);
  srl_status (*decode_package)(void*,const uint8_t*,size_t,const srl_limits*,const srl_xml_validation*,srl_xml_operation*);
} srl_xml_operation_codec;
/* Operation-aware model defaults are emitted before the XML scalar helper definition. */
static inline srl_status srl_xml_operation_string_set(srl_xml_operation*,srl_string*,const char*,size_t);
/* Select compiler-owned immutable diagnostic text without borrowing a native model. */
static inline void srl_xml_operation_select(srl_xml_operation* operation,const char* field) {
  if(operation&&field&&*field&&!operation->caller_status&&operation->failure==srl_xml_operation_none){operation->field=field;}
}
/* Preserve the first native failure classification while old entry points retain their status. */
static inline srl_status srl_xml_operation_fail(srl_xml_operation* operation,srl_status status) {
  if(operation&&!operation->caller_status&&operation->failure==srl_xml_operation_none){
    if(status==srl_limit){operation->failure=srl_xml_operation_resource_limit;}
    else if(status==srl_allocation){operation->failure=srl_xml_operation_allocation_failure;}
    else if(status!=srl_ok){operation->failure=srl_xml_operation_invalid;}
  }
  return status;
}
/* Poll before bounded native work; arbitrary caller integers are retained verbatim. */
static inline bool srl_xml_operation_check(srl_xml_operation* operation) {
  if(!operation){return true;}
  if(operation->caller_status||operation->failure!=srl_xml_operation_none){return false;}
  if(operation->poll){const int status=operation->poll(operation->userdata,operation->field?operation->field:"");if(status){operation->caller_status=status;return false;}}
  return true;
}
/* Admit logical resources before performing work or obtaining owned storage. */
static inline bool srl_xml_operation_charge(srl_xml_operation* operation,srl_xml_operation_resource resource,size_t count) {
  if(!srl_xml_operation_check(operation)){return false;}
  if(operation&&operation->consume){const int status=operation->consume(operation->userdata,resource,count,operation->field?operation->field:"");if(status){operation->caller_status=status;return false;}}
  return true;
}
/* Admit retained XML-only bytes before copying them; ordinary operations leave the hook null. */
static inline bool srl_xml_operation_preserve(srl_xml_operation* operation,size_t count) {
  if(!srl_xml_operation_check(operation)){return false;}
  if(operation&&operation->preserve){const int status=operation->preserve(operation->userdata,count,operation->field?operation->field:"");if(status){operation->caller_status=status;return false;}}
  return true;
}
/* Validate UTF8 in bounded work intervals while preserving the ordinary decoder grammar. */
static inline bool srl_xml_operation_utf8(srl_xml_operation* operation,const uint8_t* data,size_t size) {
  if(!operation){return srl_utf8(data,size);}if(size&&!data){return false;}
  const size_t interval_bytes=1024u;size_t admitted=0;
  for(size_t index=0;index<size;){
    if(index>=admitted){const size_t count=size-index<interval_bytes?size-index:interval_bytes;if(!srl_xml_operation_charge(operation,srl_xml_operation_work,count)){return false;}admitted=index+count;}
    const uint8_t first=data[index++];if(first<128u){continue;}
    size_t count;uint32_t code,minimum;
    if(first>=0xc2u&&first<=0xdfu){count=1u;code=first&31u;minimum=0x80u;}
    else if(first>=0xe0u&&first<=0xefu){count=2u;code=first&15u;minimum=0x800u;}
    else if(first>=0xf0u&&first<=0xf4u){count=3u;code=first&7u;minimum=0x10000u;}
    else{return false;}
    if(count>size-index){return false;}
    if(index+count>admitted){if(!srl_xml_operation_charge(operation,srl_xml_operation_work,index+count-admitted)){return false;}admitted=index+count;}
    while(count--){const uint8_t next=data[index++];if((next&0xc0u)!=0x80u){return false;}code=(code<<6)|(next&63u);}
    if(code<minimum||code>0x10ffffu||(code>=0xd800u&&code<=0xdfffu)){return false;}
  }
  return true;
}
/* Validate XML characters with bounded native loops and exactly the existing scalar grammar. */
static inline bool srl_xml_operation_characters(srl_xml_operation* operation,const char* text,size_t size) {
  if(!srl_xml_operation_utf8(operation,(const uint8_t*)text,size)){return false;}
  const size_t interval_bytes=1024u;
  for(size_t index=0;index<size;++index){
    if(index%interval_bytes==0u&&!srl_xml_operation_charge(operation,srl_xml_operation_work,size-index<interval_bytes?size-index:interval_bytes)){return false;}
    const unsigned char value=(unsigned char)text[index];
    if(value<32u&&value!=9u&&value!=10u&&value!=13u){return false;}
    if(value==0xefu&&size-index>=3u&&(unsigned char)text[index+1u]==0xbfu&&((unsigned char)text[index+2u]==0xbeu||(unsigned char)text[index+2u]==0xbfu)){return false;}
  }
  return true;
}
/* Sort private map indexes with bounded cancellation points; ordinary callers keep their original qsort. */
static inline bool srl_xml_operation_sort(void* data,size_t count,size_t width,int (*compare)(const void*,const void*),srl_xml_operation* operation) {
  if(count<2u){return srl_xml_operation_check(operation);}if(!operation){qsort(data,count,width,compare);return true;}
  if(!width||count>SIZE_MAX/width){srl_xml_operation_fail(operation,srl_limit);return false;}
  const size_t bytes=count*width;if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,bytes)){return false;}
  unsigned char* temporary=(unsigned char*)malloc(bytes);if(!temporary){srl_xml_operation_fail(operation,srl_allocation);return false;}
  unsigned char* source=(unsigned char*)data;unsigned char* destination=temporary;bool good=true;
  for(size_t run=1u;run<count&&good;){
    for(size_t first=0;first<count&&good;){const size_t middle=count-first<run?count:first+run;const size_t last=count-middle<run?count:middle+run;
      size_t left=first,right=middle;
      for(size_t target=first;target<last;++target){
        if(!srl_xml_operation_charge(operation,srl_xml_operation_work,width+1u)){good=false;break;}
        const bool take_left=right==last||(left<middle&&compare(source+left*width,source+right*width)<=0);
        const size_t selected=take_left?left++:right++;memcpy(destination+target*width,source+selected*width,width);
      }
      first=last;
    }
    unsigned char* swap=source;source=destination;destination=swap;
    if(run>count/2u){break;}run*=2u;
  }
  if(good&&source!=(unsigned char*)data){const size_t interval_bytes=1024u;
    for(size_t offset=0;offset<bytes;){const size_t size=bytes-offset<interval_bytes?bytes-offset:interval_bytes;
      if(!srl_xml_operation_charge(operation,srl_xml_operation_work,size)){good=false;break;}memcpy((unsigned char*)data+offset,source+offset,size);offset+=size;}
  }
  free(temporary);return good;
}
#endif
)coperation"
