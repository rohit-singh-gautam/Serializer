// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void* (__cdecl *prepare_function)(const char*,uint64_t);
typedef int32_t (__cdecl *validate_function)(const void*,const char*,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
typedef void (__cdecl *release_function)(void*);
typedef uint64_t (__cdecl *message_function)(char*,uint64_t);
typedef struct provider_bridge {
  HMODULE library;prepare_function prepare;validate_function validate;release_function release;message_function diagnostic,version;
} provider_bridge;
typedef struct prepared_provider {provider_bridge* bridge;SRWLOCK lifetime;void* handle;volatile LONG64 calls;} prepared_provider;
static const char document[]="<order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer><value>3</value></order>";
static const uint64_t default_bounds[]={16777216u,128u,100000u,100000u,8388608u};

/* Fail outside expected-rejection branches so an accepted invalid operation cannot be mistaken for success. */
static void require(bool condition,const char* message){if(!condition){fprintf(stderr,"%s\n",message);exit(EXIT_FAILURE);}}
/* Bind fixed signatures from an explicitly selected local test DLL, without changing global loader policy. */
static void load_bridge(provider_bridge* bridge,const char* path){
  const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);require(count>0,"Provider DLL UTF8 path");
  wchar_t* wide=(wchar_t*)malloc((size_t)count*sizeof(*wide));require(wide!=NULL,"Provider path allocation");
  require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,count)==count,"Provider DLL UTF16 path");
  bridge->library=LoadLibraryExW(wide,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);free(wide);require(bridge->library!=NULL,"Load local prepared provider DLL");
  const char* names[]={"serializer_xml_provider_prepare","serializer_xml_provider_validate","serializer_xml_provider_release","serializer_xml_provider_error","serializer_xml_provider_version"};
  void* destinations[]={&bridge->prepare,&bridge->validate,&bridge->release,&bridge->diagnostic,&bridge->version};
  _Static_assert(sizeof(FARPROC)==sizeof(prepare_function) && sizeof(FARPROC)==sizeof(validate_function) && sizeof(FARPROC)==sizeof(release_function) && sizeof(FARPROC)==sizeof(message_function),"Windows fixture requires uniform function-pointer representations");
  for(size_t index=0;index<sizeof(names)/sizeof(names[0]);++index){FARPROC symbol=GetProcAddress(bridge->library,names[index]);require(symbol!=NULL,"Resolve provider bridge symbol");memcpy(destinations[index],&symbol,sizeof(symbol));}
}
/* Retrieve a bounded same-OS-thread error or provider version before another calling context can replace it. */
static bool has_message(message_function function){char message[1024];const uint64_t size=function(message,sizeof(message));return size!=0 && message[0]!='\0';}
/* Prepare one immutable schema from borrowed bytes; a live bridge must outlast its owning handle. */
static bool prepare_provider(prepared_provider* provider,provider_bridge* bridge,const uint8_t* bytes,size_t size){
  memset(provider,0,sizeof(*provider));provider->bridge=bridge;InitializeSRWLock(&provider->lifetime);
  provider->handle=bridge->prepare((const char*)bytes,(uint64_t)size);return provider->handle!=NULL;
}
/* Release once after every validation read lease completes; explicit repeated close is harmless. */
static void close_provider(prepared_provider* provider){
  AcquireSRWLockExclusive(&provider->lifetime);void* handle=provider->handle;provider->handle=NULL;ReleaseSRWLockExclusive(&provider->lifetime);
  if(handle){provider->bridge->release(handle);}
}
/* Validate under a lifetime read lease, preserving every actual provider failure's nonempty bounded diagnostic. */
static srl_status bounded_validation(prepared_provider* provider,const uint8_t* bytes,size_t size,const uint64_t* bounds){
  AcquireSRWLockShared(&provider->lifetime);
  if(!provider->handle){ReleaseSRWLockShared(&provider->lifetime);return srl_invalid;}
  InterlockedIncrement64(&provider->calls);
  const int32_t status=provider->bridge->validate(provider->handle,(const char*)bytes,(uint64_t)size,bounds[0],bounds[1],bounds[2],bounds[3],bounds[4]);
  if(status!=0){require(has_message(provider->bridge->diagnostic),"Real provider failure diagnostic");}
  ReleaseSRWLockShared(&provider->lifetime);return status==0?srl_ok:status==2?srl_limit:srl_invalid;
}
/* Adapt generated complete-message limits directly to the immutable real-provider bridge. */
static srl_status validate_callback(const uint8_t* bytes,size_t size,const srl_limits* limits,void* context){
  const uint64_t bounds[]={(uint64_t)limits->max_bytes,(uint64_t)limits->max_depth,(uint64_t)limits->max_elements,(uint64_t)limits->max_elements,(uint64_t)limits->max_string_bytes};
  return bounded_validation((prepared_provider*)context,bytes,size,bounds);
}
/* Read the trusted fixture schema without retaining its source storage after preparation. */
static uint8_t* read_file(const char* path,size_t* size){
  FILE* file=fopen(path,"rb");require(file!=NULL,"Provider schema open");require(fseek(file,0,SEEK_END)==0,"Provider schema seek");
  const long count=ftell(file);require(count>=0,"Provider schema length");rewind(file);*size=(size_t)count;
  uint8_t* bytes=(uint8_t*)malloc(*size?*size:1u);require(bytes!=NULL,"Provider schema allocation");require(fread(bytes,1,*size,file)==*size,"Provider schema read");fclose(file);return bytes;
}
/* Run concurrent native calls without serializing immutable schema validation or cross-thread diagnostic buffers. */
static DWORD WINAPI worker(void* context){
  prepared_provider* provider=(prepared_provider*)context;
  for(size_t index=0;index<16u;++index){
    require(bounded_validation(provider,(const uint8_t*)document,sizeof(document)-1,default_bounds)==srl_ok,"Concurrent real provider valid input");
    require(bounded_validation(provider,(const uint8_t*)"<wrong/>",sizeof("<wrong/>")-1,default_bounds)!=srl_ok,"Concurrent real provider invalid root");
    require(bounded_validation(provider,(const uint8_t*)document,sizeof(document)-1,default_bounds)==srl_ok,"Concurrent provider retained immutable state");
  }return 0;
}
/* Qualify generated owning transactions, independent resource bounds, immutable reuse and no external acquisition. */
int main(int argc,char** argv){
  require(argc==3,"Usage: provider bridge.dll schema.xsd");provider_bridge bridge={0};load_bridge(&bridge,argv[1]);
  size_t size=0;uint8_t* bytes=read_file(argv[2],&size);prepared_provider provider;
  require(prepare_provider(&provider,&bridge,bytes,size),"Prepare real libxml2 schema");free(bytes);
  const srl_xml_validation validation={validate_callback,&provider};xml_provider_fixture_order value={0},copy={0};srl_buffer output={0},first={0},second={0};
  require(xml_provider_fixture_order_init(&value)==srl_ok && xml_provider_fixture_order_init(&copy)==srl_ok,"Native provider owning candidates");
  require(xml_provider_fixture_order_decode_xml(&value,(const uint8_t*)document,sizeof(document)-1,NULL,&validation)==srl_ok,"Generated input real provider callback");
  require(value.id==7u && srl_string_equal(&value.customer,"R&D") && value.values.size==1u && value.values.data[0]==3,"Provider generated values");
  require(xml_provider_fixture_order_encode_xml(&value,&output,NULL,&validation)==srl_ok,"Generated output real provider callback");
  require(bounded_validation(&provider,output.data,output.size,default_bounds)==srl_ok,"Prepared schema direct output reuse");
  require(xml_provider_fixture_order_decode_xml(&copy,output.data,output.size,NULL,&validation)==srl_ok,"Generated provider replay");
  require(xml_provider_fixture_order_encode(&value,srl_binary_none,&first)==srl_ok && xml_provider_fixture_order_encode(&copy,srl_binary_none,&second)==srl_ok,"Native protocol controls");
  require(first.size==second.size && memcmp(first.data,second.data,first.size)==0 && provider.calls>=4,"Provider changed native wire values or skipped callbacks");
  const char* invalid[]={"<wrong xmlns='urn:serializer:xsd'/>","<order xmlns='urn:wrong' id='7'><customer>R&amp;D</customer><value>3</value></order>","<order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer><value>3</value></order><extra/>","<order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer></order>","<!DOCTYPE order SYSTEM 'file:///must-not-open'><order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer><value>3</value></order>","<?xml version='1.1'?><order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer><value>3</value></order>"};
  for(size_t index=0;index<sizeof(invalid)/sizeof(invalid[0]);++index){require(bounded_validation(&provider,(const uint8_t*)invalid[index],strlen(invalid[index]),default_bounds)!=srl_ok,"Invalid real-provider input accepted");}
  const uint8_t malformed[]={0xc3u,0x28u};require(bounded_validation(&provider,malformed,sizeof(malformed),default_bounds)!=srl_ok,"Malformed provider UTF8 accepted");
  const char mismatch_source[]="<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:serializer:xsd'><xs:element name='order' type='xs:string'/></xs:schema>";
  prepared_provider mismatch;require(prepare_provider(&mismatch,&bridge,(const uint8_t*)mismatch_source,sizeof(mismatch_source)-1),"Prepare mismatched independent schema");
  const srl_xml_validation rejected={validate_callback,&mismatch};uint8_t* previous=output.data;const size_t previous_size=output.size;
  require(xml_provider_fixture_order_encode_xml(&value,&output,NULL,&rejected)!=srl_ok && output.data==previous && output.size==previous_size,"Failed provider output changed published buffer");
  value.id=99u;require(xml_provider_fixture_order_decode_xml(&value,(const uint8_t*)document,sizeof(document)-1,NULL,&rejected)!=srl_ok && value.id==99u,"Failed provider input changed published model");
  const char* directives[]={"include","import","redefine","override"};const char* locations[]={"file:///must-not-open.xsd","https://invalid.example/must-not-open.xsd"};
  for(size_t directive=0;directive<sizeof(directives)/sizeof(directives[0]);++directive){for(size_t location=0;location<sizeof(locations)/sizeof(locations[0]);++location){
    char source[512];const int count=snprintf(source,sizeof(source),"<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:%s schemaLocation='%s'/></xs:schema>",directives[directive],locations[location]);
    require(count>0 && (size_t)count<sizeof(source),"No-fetch schema fixture bound");prepared_provider unexpected;
    require(!prepare_provider(&unexpected,&bridge,(const uint8_t*)source,(size_t)count) && has_message(bridge.diagnostic),"Prepared schema external reference accepted");
  }}
  const char hinted[]="<order xmlns='urn:serializer:xsd' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:schemaLocation='urn:serializer:xsd https://invalid.example/must-not-open.xsd' id='7'><customer>R&amp;D</customer><value>3</value></order>";
  require(bounded_validation(&provider,(const uint8_t*)hinted,sizeof(hinted)-1,default_bounds)==srl_ok,"Provider followed untrusted schema hint");
  for(size_t index=0;index<5u;++index){uint64_t bounds[5];memcpy(bounds,default_bounds,sizeof(bounds));bounds[index]=index==0?sizeof(document)-2:index==3?0u:1u;require(bounded_validation(&provider,(const uint8_t*)document,sizeof(document)-1,bounds)!=srl_ok,"Provider resource bound ignored");}
  HANDLE workers[8];for(size_t index=0;index<sizeof(workers)/sizeof(workers[0]);++index){workers[index]=CreateThread(NULL,0,worker,&provider,0,NULL);require(workers[index]!=NULL,"Provider worker creation");}
  require(WaitForMultipleObjects((DWORD)(sizeof(workers)/sizeof(workers[0])),workers,TRUE,INFINITE)==WAIT_OBJECT_0,"Provider worker completion");
  for(size_t index=0;index<sizeof(workers)/sizeof(workers[0]);++index){require(CloseHandle(workers[index])!=0,"Provider worker handle cleanup");}
  require(has_message(bridge.version),"Real provider version unavailable");close_provider(&provider);close_provider(&provider);
  require(bounded_validation(&provider,(const uint8_t*)document,sizeof(document)-1,default_bounds)!=srl_ok,"Closed provider admitted a call");close_provider(&mismatch);
  xml_provider_fixture_order_free(&value);xml_provider_fixture_order_free(&copy);srl_buffer_free(&output);srl_buffer_free(&first);srl_buffer_free(&second);require(FreeLibrary(bridge.library)!=0,"Provider module cleanup");
  puts("C prepared libxml2 provider: owning transactions, five bounds, no-fetch, concurrent reuse and closed-state checks pass");return EXIT_SUCCESS;
}
