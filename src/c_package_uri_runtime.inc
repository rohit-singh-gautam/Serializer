// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"cpackage(
/* Recognize URI ASCII syntax independently of the active C locale. */
static inline bool @p@_uri_letter(unsigned char value){return (value>='a'&&value<='z')||(value>='A'&&value<='Z');}
/* Decode a hexadecimal URI digit without accepting locale extensions. */
static inline int @p@_uri_hex(unsigned char value){if(value>='0'&&value<='9'){return value-'0';}if(value>='a'&&value<='f'){return value-'a'+10;}if(value>='A'&&value<='F'){return value-'A'+10;}return -1;}
/* Recognize RFC 3986 unreserved and sub-delimiter bytes. */
static inline bool @p@_uri_plain(unsigned char value){return @p@_uri_letter(value)||(value>='0'&&value<='9')||(value&&strchr("-._~!$&'()*+,;=",value));}
/* Validate one already UTF-8-checked component without decoding percent escapes or changing spelling. */
static inline bool @p@_uri_component(const char* text,size_t size,const char* extra,bool query){
  for(size_t index=0;index<size;){const unsigned char first=(unsigned char)text[index++];
    if(first=='%'){if(size-index<2||@p@_uri_hex((unsigned char)text[index])<0||@p@_uri_hex((unsigned char)text[index+1])<0){return false;}index+=2;}
    else if(first>=128){uint32_t code=first;size_t remaining;if(first<0xe0u){code&=31u;remaining=1;}else if(first<0xf0u){code&=15u;remaining=2;}else{code&=7u;remaining=3;}if(remaining>size-index){return false;}while(remaining--){code=(code<<6)|((unsigned char)text[index++]&63u);}
      const bool ordinary=(code>=0xa0u&&code<=0xd7ffu)||(code>=0xf900u&&code<=0xfdcfu)||(code>=0xfdf0u&&code<=0xffefu)||(code>=0x10000u&&code<=0xdfffdu&&(code&65535u)<=0xfffdu)||(code>=0xe1000u&&code<=0xefffdu);
      const bool private_query=query&&((code>=0xe000u&&code<=0xf8ffu)||(code>=0xf0000u&&code<=0xffffdu)||(code>=0x100000u&&code<=0x10fffdu));if(!ordinary&&!private_query){return false;}
    }else if(!@p@_uri_plain(first)&&!strchr(extra,first)){return false;}
  }return true;
}
/* Validate an IPv4 tail inside a literal address without treating it as an endpoint. */
static inline bool @p@_uri_ipv4(const char* text,size_t size){size_t count=0,begin=0;while(begin<size){size_t end=begin;while(end<size&&text[end]!='.'){++end;}const size_t length=end-begin;if(!length||length>3||(length>1&&text[begin]=='0')){return false;}unsigned value=0;for(size_t index=begin;index<end;++index){if(text[index]<'0'||text[index]>'9'){return false;}value=value*10+(unsigned)(text[index]-'0');}if(value>255||++count>4){return false;}if(end==size){break;}begin=end+1;if(begin==size){return false;}}return count==4;}
/* Recognize bracketed IPv6 and IPvFuture syntax without DNS, normalization or network access. */
static inline bool @p@_uri_ip_literal(const char* text,size_t size){
  if(size>=4&&(text[0]=='v'||text[0]=='V')){size_t dot=1;while(dot<size&&text[dot]!='.'){if(@p@_uri_hex((unsigned char)text[dot])<0){return false;}++dot;}if(dot==1||dot>=size-1){return false;}for(size_t index=dot+1;index<size;++index){if(!@p@_uri_plain((unsigned char)text[index])&&text[index]!=':'){return false;}}return true;}
  size_t compression=SIZE_MAX;for(size_t index=0;index+1<size;++index){if(text[index]==':'&&text[index+1]==':'){if(compression!=SIZE_MAX){return false;}compression=index;++index;}}
  size_t groups=0,begin=0;while(begin<size){if(text[begin]==':'){if(compression==SIZE_MAX||(begin!=compression&&begin!=compression+1)){return false;}++begin;continue;}size_t end=begin;while(end<size&&text[end]!=':'){++end;}const size_t length=end-begin;
    if(memchr(text+begin,'.',length)){if(end!=size||!@p@_uri_ipv4(text+begin,length)){return false;}groups+=2;}
    else{if(!length||length>4){return false;}for(size_t index=begin;index<end;++index){if(@p@_uri_hex((unsigned char)text[index])<0){return false;}}++groups;}
    if(end==size){break;}begin=end+1;if(begin==size&&(compression==SIZE_MAX||end!=compression+1)){return false;}
  }return compression==SIZE_MAX?groups==8:groups<8;
}
/* Separate user information, host and decimal port so delimiters keep their RFC component roles. */
static inline bool @p@_uri_authority(const char* text,size_t size){
  const char* at=(const char*)memchr(text,'@',size);if(at){const size_t user=(size_t)(at-text);if(!@p@_uri_component(text,user,":",false)){return false;}text=at+1;size-=user+1;}
  const char* port=NULL;size_t port_size=0;
  if(size&&text[0]=='['){const char* close=(const char*)memchr(text,']',size);if(!close){return false;}const size_t host=(size_t)(close-text);if(!@p@_uri_ip_literal(text+1,host-1)||(host+1<size&&text[host+1]!=':')){return false;}if(host+1<size){port=text+host+2;port_size=size-host-2;}}
  else{const char* colon=(const char*)memchr(text,':',size);const size_t host=colon?(size_t)(colon-text):size;if(!@p@_uri_component(text,host,"",false)){return false;}if(colon){port=colon+1;port_size=size-host-1;}}
  for(size_t index=0;index<port_size;++index){if(port[index]<'0'||port[index]>'9'){return false;}}return true;
}
/* Validate an inert RFC 3986/3987 reference; relationship types require a fragment-free absolute IRI. */
static inline bool @p@_uri_validate(const char* value,bool absolute){
  if(!value){return false;}size_t size=strlen(value);if(size>@p@_name_bytes||!srl_utf8((const uint8_t*)value,size)){return false;}
  const char* fragment=(const char*)memchr(value,'#',size);if(fragment){const size_t prefix=(size_t)(fragment-value);if(absolute||!@p@_uri_component(fragment+1,size-prefix-1,":@/?",false)){return false;}size=prefix;}
  const char* query=(const char*)memchr(value,'?',size);if(query){const size_t prefix=(size_t)(query-value);if(!@p@_uri_component(query+1,size-prefix-1,":@/?",true)){return false;}size=prefix;}
  const char* colon=(const char*)memchr(value,':',size);const char* slash=(const char*)memchr(value,'/',size);const bool scheme=colon&&(!slash||colon<slash);
  if(scheme){const size_t prefix=(size_t)(colon-value);if(!prefix||!@p@_uri_letter((unsigned char)value[0])){return false;}for(size_t index=1;index<prefix;++index){const unsigned char byte=(unsigned char)value[index];if(!@p@_uri_letter(byte)&&!(byte>='0'&&byte<='9')&&!strchr("+-.",byte)){return false;}}value=colon+1;size-=prefix+1;}
  else if(absolute){return false;}
  if(size>=2&&value[0]=='/'&&value[1]=='/'){value+=2;size-=2;const char* end=(const char*)memchr(value,'/',size);const size_t authority=end?(size_t)(end-value):size;if(!@p@_uri_authority(value,authority)){return false;}value+=authority;size-=authority;}
  return @p@_uri_component(value,size,":@/",false);
}
/* Relationship types are absolute identifiers and never carry fragments. */
static inline bool @p@_uri(const char* value){return @p@_uri_validate(value,true);}
/* External targets may be relative, absolute or empty; their authored spelling remains unchanged. */
static inline bool @p@_iri_reference(const char* value){return @p@_uri_validate(value,false);}
)cpackage"
