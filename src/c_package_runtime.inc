// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"cpackage(
/* Owning C11 ZIP/OPC support. XML needs libxml2; DEFLATE uses the existing optional zlib profile. */
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <inttypes.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
typedef enum @p@_code { @p@_ok, @p@_invalid_zip, @p@_integrity, @p@_invalid_name,
  @p@_invalid_metadata, @p@_invalid_relationship, @p@_missing_part, @p@_semantic,
  @p@_resource_limit, @p@_allocation, @p@_codec, @p@_unavailable, @p@_invalid_options,
  @p@_cancelled, @p@_transport } @p@_code;
typedef enum @p@_resource { @p@_physical_bytes, @p@_expanded_bytes, @p@_metadata_bytes,
  @p@_entries, @p@_relationships, @p@_references, @p@_work_units, @p@_xml_nodes,
  @p@_xml_attributes, @p@_scalar_bytes, @p@_allocation_bytes, @p@_preserved_bytes,
  @p@_resource_count } @p@_resource;
enum { @p@_name_bytes=4096, @p@_field_bytes=256, @p@_chunk_bytes=32768 };
typedef struct @p@_override { const char* role; unsigned method; } @p@_override;
typedef struct @p@_budget {
  size_t maximum[@p@_resource_count],used[@p@_resource_count],max_part_bytes,max_archive_bytes,max_depth;
  bool (*is_cancelled)(void*); void* context; bool force_zip64;
  const @p@_override* overrides; size_t overrides_size;
  int status; srl_status native_status;
  char part[@p@_name_bytes+1],field[@p@_field_bytes+1],link[@p@_name_bytes+1];
} @p@_budget;
/* Begin a fresh finite operation; counters and first-error storage belong to the caller. */
static inline @p@_budget @p@_default_budget(void) {
  @p@_budget value={0}; const size_t maximum[]={256u*1024u*1024u,256u*1024u*1024u,
    16u*1024u*1024u,100000u,100000u,100000u,10000000u,1000000u,1000000u,
    64u*1024u*1024u,512u*1024u*1024u,64u*1024u*1024u};
  memcpy(value.maximum,maximum,sizeof(maximum));value.max_part_bytes=64u*1024u*1024u;
  value.max_archive_bytes=256u*1024u*1024u;value.max_depth=128;return value;
}
/* Retain the first stable error and bounded location without copying payload excerpts. */
static inline int @p@_fail(@p@_budget* budget,int status,const char* part,const char* field,const char* link) {
  if (!budget->status) {budget->status=status;snprintf(budget->part,sizeof(budget->part),"%s",part?part:"");
    snprintf(budget->field,sizeof(budget->field),"%s",field?field:"");snprintf(budget->link,sizeof(budget->link),"%s",link?link:"");}
  return budget->status;
}
/* Poll cooperative cancellation without replacing a previously retained caller error. */
static inline int @p@_poll(@p@_budget* budget) {
  if (!budget->status && budget->is_cancelled && budget->is_cancelled(budget->context)) {return @p@_fail(budget,@p@_cancelled,NULL,NULL,NULL);}return budget->status;
}
/* Admit cumulative counts before allocation, indexing or traversal. */
static inline int @p@_consume(@p@_budget* budget,@p@_resource resource,size_t count,const char* part,const char* field,const char* link) {
  if (@p@_poll(budget)) {return budget->status;}if (budget->used[resource]>budget->maximum[resource]||count>budget->maximum[resource]-budget->used[resource]) {return @p@_fail(budget,@p@_resource_limit,part,field,link);}budget->used[resource]+=count;return @p@_ok;
}
/* Copy one bounded metadata string, retaining ownership only after successful allocation. */
static inline char* @p@_string(@p@_budget* budget,const char* text,size_t size) {
  if (size>@p@_name_bytes||@p@_consume(budget,@p@_allocation_bytes,size+1,NULL,NULL,NULL)) {if(!budget->status){@p@_fail(budget,@p@_resource_limit,NULL,NULL,NULL);}return NULL;}
  char* copy=(char*)malloc(size+1);if(!copy){@p@_fail(budget,@p@_allocation,NULL,NULL,NULL);return NULL;}if(size){memcpy(copy,text,size);}copy[size]='\0';return copy;
}
/* Append admitted logical bytes, preserving transactional caller-buffer replacement. */
static inline int @p@_append(@p@_budget* budget,srl_buffer* result,const void* bytes,size_t size,size_t maximum) {
  if(result->size>maximum||size>maximum-result->size){return @p@_fail(budget,@p@_resource_limit,NULL,NULL,NULL);}
  if(@p@_consume(budget,@p@_allocation_bytes,size,NULL,NULL,NULL)){return budget->status;}
  if(!srl_reserve(result,size)){return @p@_fail(budget,@p@_allocation,NULL,NULL,NULL);}if(size){memcpy(result->data+result->size,bytes,size);}result->size+=size;return @p@_ok;
}
/* Compare ASCII URI identities independently of locale and platform case folding. */
static inline int @p@_compare(const char* left,const char* right) {for(;;++left,++right){unsigned char a=(unsigned char)*left,b=(unsigned char)*right;if(a>='A'&&a<='Z'){a+=(unsigned char)('a'-'A');}if(b>='A'&&b<='Z'){b+=(unsigned char)('a'-'A');}if(a!=b||!a){return (a>b)-(a<b);}}}
/* Recognize an equivalent parent-file prefix without folding path payload bytes in place. */
static inline bool @p@_ancestor(const char* parent,const char* child){const size_t size=strlen(parent),length=strlen(child);if(size>=length||child[size]!='/'){return false;}for(size_t index=0;index<size;++index){unsigned char a=(unsigned char)parent[index],b=(unsigned char)child[index];if(a>='A'&&a<='Z'){a+=(unsigned char)('a'-'A');}if(b>='A'&&b<='Z'){b+=(unsigned char)('a'-'A');}if(a!=b){return false;}}return true;}
/* Validate ZIP names without creating filesystem paths. */
static inline bool @p@_zip_name(const char* name) {const size_t size=strlen(name);if(!size||size>@p@_name_bytes||name[0]=='/'){return false;}size_t start=0;for(size_t index=0;index<=size;++index){const unsigned char byte=(unsigned char)name[index];if(byte=='\\'||byte==':'){return false;}if(byte=='/'||!byte){const size_t length=index-start;if(!length||(length==1&&name[start]=='.')||(length==2&&name[start]=='.'&&name[start+1]=='.')){return false;}start=index+1;}}return true;}
/* Require escaped canonical absolute OPC part URIs; equivalent names compare case-insensitively. */
static inline bool @p@_part_name(const char* name) {if(!name||name[0]!='/'||!@p@_zip_name(name+1)){return false;}for(size_t index=1;name[index];++index){const unsigned char byte=(unsigned char)name[index];if(byte<=32||byte>=127||strchr("?[]#",byte)){return false;}if(byte=='.'&&(name[index+1]=='/'||!name[index+1])){return false;}if(byte=='%'){unsigned value=0;for(size_t digit=0;digit<2;++digit){const unsigned char code=(unsigned char)name[++index];if(code>='0'&&code<='9'){value=value*16+code-'0';}else if(code>='a'&&code<='f'){value=value*16+code-'a'+10;}else if(code>='A'&&code<='F'){value=value*16+code-'A'+10;}else{return false;}}if((value>='a'&&value<='z')||(value>='A'&&value<='Z')||(value>='0'&&value<='9')||strchr("-._~/\\",(int)value)){return false;}}}return true;}
/* Resolve relative internal links while rejecting traversal beyond the package root. */
static inline char* @p@_target(@p@_budget* budget,const char* source,const char* target) {if(!target||!*target||strchr(target,'\\')||!strncmp(target,"//",2)){ @p@_fail(budget,@p@_invalid_relationship,source,NULL,NULL);return NULL;}char path[@p@_name_bytes+1];size_t length=0;if(*target!='/'){const char* slash=strrchr(source,'/');length=slash?(size_t)(slash-source):0;if(length){memcpy(path,source,length);}}const char* cursor=target;if((*cursor=='#'||*cursor=='?')&&strcmp(source,"/")){return @p@_string(budget,source,strlen(source));}while(*cursor&&*cursor!='#'&&*cursor!='?'){while(*cursor=='/'){++cursor;}const char* begin=cursor;while(*cursor&&*cursor!='/'&&*cursor!='#'&&*cursor!='?'){++cursor;}const size_t count=(size_t)(cursor-begin);if(!count||(count==1&&*begin=='.')){continue;}if(count==2&&begin[0]=='.'&&begin[1]=='.'){if(!length){@p@_fail(budget,@p@_invalid_relationship,source,NULL,NULL);return NULL;}while(length&&path[length-1]!='/'){--length;}if(length){--length;}continue;}if(count>sizeof(path)-length-2){@p@_fail(budget,@p@_resource_limit,source,NULL,NULL);return NULL;}path[length++]='/';memcpy(path+length,begin,count);length+=count;}path[length]='\0';if(!@p@_part_name(path)){@p@_fail(budget,@p@_invalid_relationship,source,NULL,NULL);return NULL;}return @p@_string(budget,path,length);}
/* Require inert absolute URIs for relationship types and external targets. */
static inline bool @p@_uri(const char* value) {if(!value||strlen(value)>@p@_name_bytes){return false;}const char* colon=strchr(value,':');if(!colon||colon==value||!colon[1]){return false;}for(const char* cursor=value;cursor<colon;++cursor){const unsigned char byte=(unsigned char)*cursor;if(!((byte>='a'&&byte<='z')||(byte>='A'&&byte<='Z')||(cursor>value&&((byte>='0'&&byte<='9')||strchr("+-.",byte))))){return false;}}for(;*value;++value){if((unsigned char)*value<=32){return false;}}return true;}
/* Recognize the exact MIME token alphabet independently of locale. */
static inline bool @p@_mime_token(unsigned char value){return (value>='a'&&value<='z')||(value>='A'&&value<='Z')||(value>='0'&&value<='9')||(value&&strchr("!#$%&'*+-.^_`|~",value));}
typedef struct @p@_mime_parameter {const char* name;size_t name_size;const char* value;size_t value_size;bool quoted;} @p@_mime_parameter;
/* Compare bounded ASCII names without copying or folding parameter values. */
static inline bool @p@_ascii_equal(const char* left,size_t left_size,const char* right,size_t right_size){if(left_size!=right_size){return false;}for(size_t index=0;index<left_size;++index){unsigned char a=(unsigned char)left[index],b=(unsigned char)right[index];if(a>='A'&&a<='Z'){a+=(unsigned char)('a'-'A');}if(b>='A'&&b<='Z'){b+=(unsigned char)('a'-'A');}if(a!=b){return false;}}return true;}
/* Select one complete type/subtype prefix without accepting extra slashes or leading whitespace. */
static inline const char* @p@_mime_base(const char* value){if(!value||strlen(value)>@p@_name_bytes){return NULL;}const char* cursor=value;while(@p@_mime_token((unsigned char)*cursor)){++cursor;}if(cursor==value||*cursor++!='/'){return NULL;}const char* subtype=cursor;while(@p@_mime_token((unsigned char)*cursor)){++cursor;}return cursor==subtype?NULL:cursor;}
/* Parse one parameter while preserving quoted whitespace, escapes and case-sensitive values. */
static inline int @p@_mime_next(const char** position,@p@_mime_parameter* result){const char* cursor=*position;if(!*cursor){return 0;}while(*cursor==' '||*cursor=='\t'){++cursor;}if(*cursor++!=';'){return -1;}while(*cursor==' '||*cursor=='\t'){++cursor;}result->name=cursor;while(@p@_mime_token((unsigned char)*cursor)){++cursor;}result->name_size=(size_t)(cursor-result->name);if(!result->name_size||*cursor++!='='){return -1;}result->quoted=*cursor=='"';if(result->quoted){result->value=++cursor;while(*cursor&&*cursor!='"'){unsigned char value=(unsigned char)*cursor++;if(value=='\\'){if(!*cursor){return -1;}value=(unsigned char)*cursor++;}if((value<32&&value!='\t')||value==127){return -1;}}if(*cursor!='"'){return -1;}result->value_size=(size_t)(cursor-result->value);++cursor;}else{result->value=cursor;while(@p@_mime_token((unsigned char)*cursor)){++cursor;}result->value_size=(size_t)(cursor-result->value);if(!result->value_size){return -1;}}*position=cursor;return 1;}
/* Compare decoded parameter values, permitting equivalent token/quoted spellings only. */
static inline bool @p@_mime_value_equal(const @p@_mime_parameter* left,const @p@_mime_parameter* right){size_t a=0,b=0;while(a<left->value_size&&b<right->value_size){unsigned char x=(unsigned char)left->value[a++],y=(unsigned char)right->value[b++];if(left->quoted&&x=='\\'){x=(unsigned char)left->value[a++];}if(right->quoted&&y=='\\'){y=(unsigned char)right->value[b++];}if(x!=y){return false;}}return a==left->value_size&&b==right->value_size;}
/* Validate complete MIME syntax and reject duplicate names or reserved OPC media parameters. */
static inline bool @p@_mime(const char* value){const char* base=@p@_mime_base(value);if(!base){return false;}const char* cursor=base;@p@_mime_parameter parameter;int status;while((status=@p@_mime_next(&cursor,&parameter))==1){const char* previous=base;@p@_mime_parameter old;while(previous<parameter.name){if(@p@_mime_next(&previous,&old)!=1){return false;}if(old.name==parameter.name){break;}if(@p@_ascii_equal(old.name,old.name_size,parameter.name,parameter.name_size)){return false;}}}if(status<0){return false;}if(base!=cursor&&(@p@_ascii_equal(value,(size_t)(base-value),"application/vnd.openxmlformats-package.relationships+xml",sizeof("application/vnd.openxmlformats-package.relationships+xml")-1)||@p@_ascii_equal(value,(size_t)(base-value),"application/vnd.openxmlformats-package.core-properties+xml",sizeof("application/vnd.openxmlformats-package.core-properties+xml")-1))){return false;}return true;}
/* Compare normalized media and parameter names while preserving parameter value identity. */
static inline bool @p@_mime_equal(const char* left,const char* right){if(!@p@_mime(left)||!@p@_mime(right)){return false;}const char* a=@p@_mime_base(left);const char* b=@p@_mime_base(right);if(!@p@_ascii_equal(left,(size_t)(a-left),right,(size_t)(b-right))){return false;}size_t left_count=0,right_count=0;@p@_mime_parameter x,y;const char* cursor=a;while(@p@_mime_next(&cursor,&x)==1){++left_count;const char* target=b;bool found=false;while(@p@_mime_next(&target,&y)==1){if(@p@_ascii_equal(x.name,x.name_size,y.name,y.name_size)){found=@p@_mime_value_equal(&x,&y);break;}}if(!found){return false;}}while(@p@_mime_next(&b,&y)==1){++right_count;}return left_count==right_count;}
/* Read fixed little-endian wire integers without out-of-bounds access. */
typedef struct @p@_wire {const uint8_t* data;size_t size;bool valid;} @p@_wire;
static inline uint64_t @p@_number(@p@_wire* wire,size_t offset,size_t count) {if(count>8||offset>wire->size||count>wire->size-offset){wire->valid=false;return 0;}uint64_t value=0;for(size_t index=0;index<count;++index){value|=(uint64_t)wire->data[offset+index]<<(index*8);}return value;}
/* Decode a complete checked source span before converting offsets to size_t. */
static inline bool @p@_span(@p@_wire* wire,uint64_t offset,uint64_t size) {if(offset>wire->size||size>wire->size-(size_t)offset){wire->valid=false;return false;}return true;}
/* Calculate standard CRC32 with constant workspace and bounded cancellation intervals. */
static inline uint32_t @p@_crc(const uint8_t* bytes,size_t size,@p@_budget* budget) {uint32_t value=UINT32_MAX;for(size_t index=0;index<size;++index){if(index%4096==0&&@p@_poll(budget)){return 0;}value^=bytes[index];for(unsigned bit=0;bit<8;++bit){value=(value>>1)^((value&1)?UINT32_C(0xedb88320):0);}}return ~value;}
typedef struct @p@_entry {char* name;uint64_t compressed,expanded,offset;size_t data_offset,end_offset;uint32_t crc;unsigned method,flags;} @p@_entry;
typedef struct @p@_archive {const uint8_t* data;size_t size;@p@_entry* entries;size_t entries_size;} @p@_archive;
/* Release only reader-owned directory state; the physical byte source remains borrowed. */
)cpackage"
R"cpackage(static inline void @p@_archive_free(@p@_archive* archive) {if(archive->entries){for(size_t index=0;index<archive->entries_size;++index){free(archive->entries[index].name);}}free(archive->entries);memset(archive,0,sizeof(*archive));}
/* Admit comparison work before sorting directory or producer inventories. */
static inline int @p@_sort_work(size_t count,@p@_budget* budget){size_t levels=0;for(size_t remaining=count;remaining>1;remaining=remaining/2+remaining%2){++levels;}if(count>SIZE_MAX/(levels+1)){return @p@_fail(budget,@p@_resource_limit,NULL,NULL,NULL);}return @p@_consume(budget,@p@_work_units,count*(levels+1),NULL,NULL,NULL);}
/* Compare immutable source directory names without quadratic pair scans. */
static inline int @p@_entry_name_compare(const void* left,const void* right){const @p@_entry* a=*(const @p@_entry*const*)left;const @p@_entry* b=*(const @p@_entry*const*)right;return @p@_compare(a->name,b->name);}
/* Order admitted local spans to reject overlapping records in O(n log n) work. */
static inline int @p@_entry_offset_compare(const void* left,const void* right) {const @p@_entry* a=*(const @p@_entry*const*)left;const @p@_entry* b=*(const @p@_entry*const*)right;return (a->offset>b->offset)-(a->offset<b->offset);}
/* Parse ZIP32/ZIP64 directory/local inventories and signed/unsigned descriptor extents transactionally. */
static inline int @p@_archive_open(@p@_archive* destination,const uint8_t* bytes,size_t size,@p@_budget* budget) {
  @p@_archive fresh={0};@p@_wire wire={bytes,size,true};size_t end=SIZE_MAX;
  if((size&&!bytes)||size<22||size>budget->max_archive_bytes||@p@_consume(budget,@p@_physical_bytes,size,NULL,NULL,NULL)){goto invalid;}
  const size_t first=size>22+UINT16_MAX?size-22-UINT16_MAX:0;for(size_t at=size-22;;--at){if(@p@_number(&wire,at,4)==UINT32_C(0x06054b50)&&@p@_number(&wire,at+20,2)==size-at-22){end=at;break;}if(at==first){break;}}
  if(end==SIZE_MAX||@p@_number(&wire,end+4,2)||@p@_number(&wire,end+6,2)||@p@_number(&wire,end+8,2)!=@p@_number(&wire,end+10,2)){goto invalid;}
  uint64_t count=@p@_number(&wire,end+10,2),directory_size=@p@_number(&wire,end+12,4),directory=@p@_number(&wire,end+16,4);size_t directory_end=end;
  if(directory_size==UINT32_MAX||directory==UINT32_MAX||(count==UINT16_MAX&&end>=20&&@p@_number(&wire,end-20,4)==UINT32_C(0x07064b50))){if(end<20||@p@_number(&wire,end-20,4)!=UINT32_C(0x07064b50)||@p@_number(&wire,end-16,4)||@p@_number(&wire,end-4,4)!=1){goto invalid;}const uint64_t z64=@p@_number(&wire,end-12,8);if(!@p@_span(&wire,z64,56)){goto invalid;}const size_t z=(size_t)z64;const uint64_t length=@p@_number(&wire,z+4,8);if(@p@_number(&wire,z,4)!=UINT32_C(0x06064b50)||length<44||!@p@_span(&wire,z64+12,length)||z64+12+length!=end-20||@p@_number(&wire,z+16,4)||@p@_number(&wire,z+20,4)||@p@_number(&wire,z+24,8)!=@p@_number(&wire,z+32,8)){goto invalid;}count=@p@_number(&wire,z+32,8);directory_size=@p@_number(&wire,z+40,8);directory=@p@_number(&wire,z+48,8);directory_end=z;}
  if(!@p@_span(&wire,directory,directory_size)||directory+directory_size!=directory_end||count>directory_size/46||count>SIZE_MAX/sizeof(@p@_entry)||@p@_consume(budget,@p@_entries,(size_t)count,NULL,NULL,NULL)||@p@_consume(budget,@p@_allocation_bytes,(size_t)count*sizeof(@p@_entry),NULL,NULL,NULL)){goto invalid;}
  fresh.data=bytes;fresh.size=size;fresh.entries_size=(size_t)count;fresh.entries=(@p@_entry*)calloc(count?((size_t)count):1,sizeof(@p@_entry));if(!fresh.entries){@p@_fail(budget,@p@_allocation,NULL,NULL,NULL);goto invalid;}size_t cursor=(size_t)directory;
  for(size_t index=0;index<fresh.entries_size;++index){@p@_entry* entry=&fresh.entries[index];if(@p@_consume(budget,@p@_work_units,1,NULL,NULL,NULL)||cursor>directory_end||directory_end-cursor<46||@p@_number(&wire,cursor,4)!=UINT32_C(0x02014b50)){goto invalid;}entry->flags=(unsigned)@p@_number(&wire,cursor+8,2);entry->method=(unsigned)@p@_number(&wire,cursor+10,2);if((entry->flags&~0x0808u)||(entry->method!=0&&entry->method!=8)||@p@_number(&wire,cursor+34,2)){goto invalid;}const size_t n=(size_t)@p@_number(&wire,cursor+28,2),x=(size_t)@p@_number(&wire,cursor+30,2),c=(size_t)@p@_number(&wire,cursor+32,2);if(n+x+c>directory_end-cursor-46||n>@p@_name_bytes||!srl_utf8(bytes+cursor+46,n)||memchr(bytes+cursor+46,0,n)){goto invalid;}entry->name=@p@_string(budget,(const char*)bytes+cursor+46,n);if(!entry->name||!@p@_zip_name(entry->name)){goto invalid;}
    entry->compressed=@p@_number(&wire,cursor+20,4);entry->expanded=@p@_number(&wire,cursor+24,4);entry->offset=@p@_number(&wire,cursor+42,4);entry->crc=(uint32_t)@p@_number(&wire,cursor+16,4);bool found=false;size_t extra=cursor+46+n;const size_t extra_end=extra+x;const bool zip64=entry->compressed==UINT32_MAX||entry->expanded==UINT32_MAX||entry->offset==UINT32_MAX;
    while(extra<extra_end){if(extra_end-extra<4){goto invalid;}const unsigned tag=(unsigned)@p@_number(&wire,extra,2);const size_t length=(size_t)@p@_number(&wire,extra+2,2);extra+=4;if(length>extra_end-extra){goto invalid;}if(tag==1){if(found){goto invalid;}found=true;size_t at=extra;uint64_t* values[]={&entry->expanded,&entry->compressed,&entry->offset};for(size_t value=0;value<sizeof(values)/sizeof(values[0]);++value){if(*values[value]==UINT32_MAX){if(extra+length-at<8){goto invalid;}*values[value]=@p@_number(&wire,at,8);at+=8;}}}extra+=length;}
    if((zip64&&!found)||entry->expanded>budget->max_part_bytes||!@p@_span(&wire,entry->offset,30)||entry->offset>=directory){goto invalid;}const size_t local=(size_t)entry->offset;if(@p@_number(&wire,local,4)!=UINT32_C(0x04034b50)||@p@_number(&wire,local+6,2)!=entry->flags||@p@_number(&wire,local+8,2)!=entry->method){goto invalid;}const size_t ln=(size_t)@p@_number(&wire,local+26,2),lx=(size_t)@p@_number(&wire,local+28,2);if(!@p@_span(&wire,entry->offset,30+ln+lx)||ln!=n||memcmp(bytes+local+30,entry->name,n)){goto invalid;}entry->data_offset=local+30+ln+lx;if(!@p@_span(&wire,entry->data_offset,entry->compressed)){goto invalid;}entry->end_offset=entry->data_offset+(size_t)entry->compressed;
    if(entry->flags&8){size_t descriptor=0;for(size_t width_index=0;width_index<2&&!descriptor;++width_index){const size_t width=(zip64?width_index==0:width_index!=0)?8:4;for(size_t signed_form=0;signed_form<2;++signed_form){const size_t length=4+width*2+signed_form*4,at=entry->end_offset+signed_form*4;if(entry->end_offset>directory||length>directory-entry->end_offset){continue;}if((!signed_form||@p@_number(&wire,entry->end_offset,4)==UINT32_C(0x08074b50))&&@p@_number(&wire,at,4)==entry->crc&&@p@_number(&wire,at+4,width)==entry->compressed&&@p@_number(&wire,at+4+width,width)==entry->expanded){descriptor=entry->end_offset+length;break;}}}if(!descriptor){goto invalid;}entry->end_offset=descriptor;
    }else{uint64_t compressed=@p@_number(&wire,local+18,4),expanded=@p@_number(&wire,local+22,4);if(@p@_number(&wire,local+14,4)!=entry->crc){goto invalid;}size_t at=local+30+ln;const size_t finish=at+lx;while(at<finish){if(finish-at<4){goto invalid;}const unsigned tag=(unsigned)@p@_number(&wire,at,2);const size_t length=(size_t)@p@_number(&wire,at+2,2);at+=4;if(length>finish-at){goto invalid;}if(tag==1){size_t value_at=at;uint64_t* values[]={&expanded,&compressed};for(size_t value=0;value<2;++value){if(*values[value]==UINT32_MAX){if(at+length-value_at<8){goto invalid;}*values[value]=@p@_number(&wire,value_at,8);value_at+=8;}}}at+=length;}if(compressed!=entry->compressed||expanded!=entry->expanded){goto invalid;}}
    if(entry->end_offset>directory||!wire.valid){goto invalid;}cursor+=46+n+x+c;
  }
  if(cursor!=directory_end||!wire.valid){goto invalid;}if(count){if(@p@_consume(budget,@p@_allocation_bytes,(size_t)count*sizeof(@p@_entry*),NULL,NULL,NULL)){goto invalid;}@p@_entry** spans=(@p@_entry**)malloc((size_t)count*sizeof(*spans));if(!spans){@p@_fail(budget,@p@_allocation,NULL,NULL,NULL);goto invalid;}for(size_t index=0;index<(size_t)count;++index){spans[index]=&fresh.entries[index];}if(@p@_sort_work((size_t)count,budget)){free(spans);goto invalid;}qsort(spans,(size_t)count,sizeof(*spans),@p@_entry_name_compare);for(size_t name=1;name<(size_t)count;++name){if(!@p@_compare(spans[name-1]->name,spans[name]->name)){@p@_fail(budget,@p@_invalid_name,spans[name]->name,NULL,NULL);break;}}if(budget->status){free(spans);goto invalid;}qsort(spans,(size_t)count,sizeof(*spans),@p@_entry_offset_compare);bool overlap=false;for(size_t index=1;index<(size_t)count;++index){if(spans[index-1]->end_offset>spans[index]->offset){overlap=true;break;}}free(spans);if(overlap){goto invalid;}}
  @p@_archive_free(destination);*destination=fresh;return @p@_poll(budget);
invalid:@p@_archive_free(&fresh);return @p@_fail(budget,@p@_invalid_zip,NULL,NULL,NULL);
}
/* Expand one independently indexed payload, checking CRC/size and exact provider consumption. */
static inline int @p@_archive_read(const @p@_archive* archive,size_t index,srl_buffer* destination,@p@_budget* budget) {if(index>=archive->entries_size){return @p@_fail(budget,@p@_missing_part,NULL,NULL,NULL);}const @p@_entry* entry=&archive->entries[index];if(@p@_consume(budget,@p@_expanded_bytes,(size_t)entry->expanded,entry->name,NULL,NULL)||@p@_consume(budget,@p@_allocation_bytes,(size_t)entry->expanded,entry->name,NULL,NULL)){return budget->status;}srl_buffer fresh={0};@c@_decode_options options=@c@_default_decode_options();options.format=entry->method?@c@_deflate:@c@_none;options.max_input_bytes=(size_t)entry->compressed;options.max_output_bytes=(size_t)entry->expanded;options.is_cancelled=budget->is_cancelled;options.context=budget->context;const @c@_code code=@c@_decompress(archive->data+entry->data_offset,(size_t)entry->compressed,options,&fresh);if(code!=@c@_ok){srl_buffer_free(&fresh);return @p@_fail(budget,code==@c@_unavailable?@p@_unavailable:code==@c@_cancelled?@p@_cancelled:@p@_integrity,entry->name,NULL,NULL);}if(fresh.size!=entry->expanded||@p@_crc(fresh.data,fresh.size,budget)!=entry->crc){srl_buffer_free(&fresh);return @p@_fail(budget,@p@_integrity,entry->name,NULL,NULL);}srl_buffer_free(destination);*destination=fresh;return @p@_poll(budget);}
typedef struct @p@_binary_part {char* name;char* content_type;srl_buffer bytes;size_t source_index;unsigned method;} @p@_binary_part;
typedef struct @p@_relationship {char* source;char* id;char* type;char* target;bool external;} @p@_relationship;
typedef struct @p@_extensions {@p@_binary_part* parts;size_t parts_size;@p@_relationship* links;size_t links_size;} @p@_extensions;
/* Release an owning opaque part and its associated discovered metadata. */
static inline void @p@_binary_part_free(@p@_binary_part* part){free(part->name);free(part->content_type);srl_buffer_free(&part->bytes);memset(part,0,sizeof(*part));}
/* Release one owning source-local relationship. */
static inline void @p@_relationship_free(@p@_relationship* link){free(link->source);free(link->id);free(link->type);free(link->target);memset(link,0,sizeof(*link));}
/* Release only extension resources owned by this aggregate. */
static inline void @p@_extensions_free(@p@_extensions* value){for(size_t index=0;index<value->parts_size;++index){@p@_binary_part_free(&value->parts[index]);}for(size_t index=0;index<value->links_size;++index){@p@_relationship_free(&value->links[index]);}free(value->parts);free(value->links);memset(value,0,sizeof(*value));}
/* Append one explicit little-endian wire word. */
static inline int @p@_word(@p@_budget* budget,srl_buffer* result,uint64_t value,size_t width){uint8_t bytes[sizeof(value)];for(size_t index=0;index<width;++index){bytes[index]=(uint8_t)(value>>(index*8));}return @p@_append(budget,result,bytes,width,budget->max_archive_bytes);}
/* Order immutable producer parts by exact UTF-8 name bytes. */
static inline int @p@_part_compare(const void* left,const void* right){return strcmp((*(const @p@_binary_part*const*)left)->name,(*(const @p@_binary_part*const*)right)->name);}
/* Validate equivalent producer names through a sorted immutable pointer inventory. */
static inline int @p@_part_identity_compare(const void* left,const void* right){return @p@_compare((*(const @p@_binary_part*const*)left)->name,(*(const @p@_binary_part*const*)right)->name);}
/* Emit canonical ZIP32/forced-ZIP64 records with native stored/raw-DEFLATE providers. */
static inline int @p@_zip_write(const @p@_binary_part* parts,size_t count,srl_buffer* destination,@p@_budget* budget){srl_buffer fresh={0},directory={0};if(count>SIZE_MAX/sizeof(@p@_binary_part*)){return @p@_fail(budget,@p@_resource_limit,NULL,NULL,NULL);}if(@p@_consume(budget,@p@_entries,count,NULL,NULL,NULL)){return budget->status;}if(@p@_consume(budget,@p@_allocation_bytes,(count?count:1)*sizeof(@p@_binary_part*),NULL,NULL,NULL)){return budget->status;}const @p@_binary_part** ordered=(const @p@_binary_part**)malloc((count?count:1)*sizeof(*ordered));if(!ordered){return @p@_fail(budget,@p@_allocation,NULL,NULL,NULL);}for(size_t index=0;index<count;++index){if(!parts||!parts[index].name||!@p@_zip_name(parts[index].name)){free(ordered);return @p@_fail(budget,@p@_invalid_name,NULL,NULL,NULL);}ordered[index]=&parts[index];}if(@p@_sort_work(count,budget)){free(ordered);return budget->status;}qsort(ordered,count,sizeof(*ordered),@p@_part_identity_compare);for(size_t index=1;index<count;++index){if(!@p@_compare(ordered[index-1]->name,ordered[index]->name)){free(ordered);return @p@_fail(budget,@p@_invalid_name,NULL,NULL,NULL);}}qsort(ordered,count,sizeof(*ordered),@p@_part_compare);
  for(size_t index=0;index<count&&!budget->status;++index){const @p@_binary_part* part=ordered[index];if(!part->name||!@p@_zip_name(part->name)||(index&&!@p@_compare(part->name,ordered[index-1]->name))||(part->method!=0&&part->method!=8)||part->bytes.size>budget->max_part_bytes){@p@_fail(budget,@p@_invalid_name,part->name,NULL,NULL);break;}if(@p@_consume(budget,@p@_expanded_bytes,part->bytes.size,part->name,NULL,NULL)){break;}@c@_encode_options options=@c@_default_encode_options();options.format=part->method?@c@_deflate:@c@_none;options.max_input_bytes=part->bytes.size;options.max_output_bytes=budget->max_archive_bytes;options.is_cancelled=budget->is_cancelled;options.context=budget->context;srl_buffer packed={0};const @c@_code code=@c@_compress(part->bytes.data,part->bytes.size,options,&packed);if(code!=@c@_ok){@p@_fail(budget,code==@c@_unavailable?@p@_unavailable:code==@c@_cancelled?@p@_cancelled:@p@_codec,part->name,NULL,NULL);srl_buffer_free(&packed);break;}const uint32_t checksum=@p@_crc(part->bytes.data,part->bytes.size,budget);const uint64_t offset=fresh.size;const size_t name_size=strlen(part->name);const bool large=budget->force_zip64||fresh.size>=UINT32_MAX||packed.size>=UINT32_MAX||part->bytes.size>=UINT32_MAX;const uint64_t values[]={UINT32_C(0x04034b50),large?45u:20u,0x800u,part->method,0,33,checksum,large?UINT32_MAX:packed.size,large?UINT32_MAX:part->bytes.size,name_size,large?20u:0u};const size_t widths[]={4,2,2,2,2,2,4,4,4,2,2};for(size_t value=0;value<sizeof(values)/sizeof(values[0]);++value){@p@_word(budget,&fresh,values[value],widths[value]);}@p@_append(budget,&fresh,part->name,name_size,budget->max_archive_bytes);if(large){@p@_word(budget,&fresh,1,2);@p@_word(budget,&fresh,16,2);@p@_word(budget,&fresh,part->bytes.size,8);@p@_word(budget,&fresh,packed.size,8);}@p@_append(budget,&fresh,packed.data,packed.size,budget->max_archive_bytes);
    const uint64_t central[]={UINT32_C(0x02014b50),0x033fu,large?45u:20u,0x800u,part->method,0,33,checksum,large?UINT32_MAX:packed.size,large?UINT32_MAX:part->bytes.size,name_size,large?28u:0u,0,0,0,UINT32_C(0100644)<<16,large?UINT32_MAX:offset};const size_t central_widths[]={4,2,2,2,2,2,2,4,4,4,2,2,2,2,2,4,4};for(size_t value=0;value<sizeof(central)/sizeof(central[0]);++value){@p@_word(budget,&directory,central[value],central_widths[value]);}@p@_append(budget,&directory,part->name,name_size,budget->max_archive_bytes);if(large){@p@_word(budget,&directory,1,2);@p@_word(budget,&directory,24,2);@p@_word(budget,&directory,part->bytes.size,8);@p@_word(budget,&directory,packed.size,8);@p@_word(budget,&directory,offset,8);}srl_buffer_free(&packed);
  }
)cpackage"
R"cpackage(  const uint64_t offset=fresh.size,wide_size=directory.size;@p@_append(budget,&fresh,directory.data,directory.size,budget->max_archive_bytes);const bool large=budget->force_zip64||count>=UINT16_MAX||offset>=UINT32_MAX||wide_size>=UINT32_MAX;if(large){const uint64_t z=fresh.size;const uint64_t values[]={UINT32_C(0x06064b50),44,45,45,0,0,count,count,wide_size,offset,UINT32_C(0x07064b50),0,z,1};const size_t widths[]={4,8,2,2,4,4,8,8,8,8,4,4,8,4};for(size_t index=0;index<sizeof(values)/sizeof(values[0]);++index){@p@_word(budget,&fresh,values[index],widths[index]);}}
  const uint64_t values[]={UINT32_C(0x06054b50),0,0,large?UINT16_MAX:count,large?UINT16_MAX:count,large?UINT32_MAX:wide_size,large?UINT32_MAX:offset,0};const size_t widths[]={4,2,2,2,2,4,4,2};for(size_t index=0;index<sizeof(values)/sizeof(values[0]);++index){@p@_word(budget,&fresh,values[index],widths[index]);}free(ordered);srl_buffer_free(&directory);if(@p@_consume(budget,@p@_physical_bytes,fresh.size,NULL,NULL,NULL)){srl_buffer_free(&fresh);return budget->status;}srl_buffer_free(destination);*destination=fresh;return @p@_ok;
}
)cpackage"
