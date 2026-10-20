// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"

enum { resource_count = srl_xml_operation_references + 1, caller_marker = -70123 };
static size_t assertions;
typedef struct ledger {
  size_t used[resource_count], maximum[resource_count], allocations, deny_allocation;
  struct ledger* identity;
  bool deny_default, deny_factory;
} ledger;

/* Stop at the first independently specified native ownership or accounting mismatch. */
static void require(bool value,const char* message) {
  ++assertions;if(!value){fprintf(stderr,"%s\n",message);exit(EXIT_FAILURE);}
}
/* Initialize one independently owned callback ledger, including a userdata identity control. */
static void ledger_init(ledger* value) {
  memset(value,0,sizeof(*value));value->identity=value;
  for(size_t index=0;index<resource_count;++index){value->maximum[index]=SIZE_MAX;}
}
/* Reject exact shared admissions or known/factory default construction with one arbitrary caller integer. */
static int consume(void* userdata,srl_xml_operation_resource resource,size_t count,const char* field) {
  ledger* value=(ledger*)userdata;require(value->identity==value,"Caller userdata changed");
  require((unsigned)resource<resource_count,"Unknown native resource");
  if(resource==srl_xml_operation_allocation){++value->allocations;
    if(value->deny_allocation&&value->allocations==value->deny_allocation){return caller_marker;}
    if(value->deny_default&&strcmp(field,"operation_policy::child.wireValue")==0&&count==sizeof("known default storage")){return caller_marker;}
    if(value->deny_factory&&strcmp(field,"operation_policy::factory_child.label")==0&&count==sizeof("factory retained string")){return caller_marker;}
  }
  if(value->used[resource]>value->maximum[resource]||count>value->maximum[resource]-value->used[resource]){return caller_marker;}
  value->used[resource]+=count;return 0;
}
/* Keep polling synchronous and verify the operation has retained the exact caller object. */
static int poll(void* userdata,const char* field) {
  ledger* value=(ledger*)userdata;(void)field;require(value->identity==value,"Poll userdata changed");return 0;
}
/* Return a fresh operation; policy and ledger storage remain owned by the enclosing test call. */
static srl_xml_operation operation_for(ledger* value) {
  srl_xml_operation operation={0};operation.consume=consume;operation.poll=poll;operation.userdata=value;return operation;
}

typedef struct policy_case {
  const char* name;size_t size;
  srl_status (*init)(void*);srl_status (*init_operation)(void*,srl_xml_operation*);void (*free_value)(void*);
  srl_status (*binary)(const void*,srl_buffer*);srl_status (*xml)(const void*,srl_buffer*,const srl_limits*);
  const srl_xml_operation_codec* (*codec)(void);
} policy_case;

/* Adapt exact existing native function signatures to the shared test harness without function-pointer casts. */
#define DEFINE_CASE(label,type) \
  static srl_status label##_init(void* value){return type##_init((type*)value);} \
  static srl_status label##_init_operation(void* value,srl_xml_operation* operation){return srl_xml_operation_init_##type((type*)value,operation);} \
  static void label##_free(void* value){type##_free((type*)value);} \
  static srl_status label##_binary(const void* value,srl_buffer* bytes){return type##_encode((const type*)value,srl_binary_none,bytes);} \
  static srl_status label##_xml(const void* value,srl_buffer* bytes,const srl_limits* limits){return type##_encode_xml((const type*)value,bytes,limits);} \
  static const policy_case label##_case={#label,sizeof(type),label##_init,label##_init_operation,label##_free,label##_binary,label##_xml,srl_xml_operation_codec_##type}
DEFINE_CASE(scalar,operation_policy_scalar);
DEFINE_CASE(map,operation_policy_maps);
DEFINE_CASE(mixed,operation_policy_mixed);
DEFINE_CASE(digest,operation_policy_digests);
DEFINE_CASE(defaults,operation_policy_defaults);
DEFINE_CASE(factory,operation_policy_factory_defaults);
#undef DEFINE_CASE

/* Allocate initialized test-owned native storage independently of the operation's private candidate. */
static void* model_new(const policy_case* selected) {
  void* value=calloc(1u,selected->size);require(value!=NULL,"Model fixture allocation");
  require(selected->init(value)==srl_ok,"Ordinary init compatibility");return value;
}
/* Free only the ownership represented by one initialized fixture value. */
static void model_delete(const policy_case* selected,void* value) {selected->free_value(value);free(value);}
/* Compare the whole initialized destination and its ordinary native bytes after a rejected operation. */
static void unchanged(const policy_case* selected,const void* value,const void* snapshot,const srl_buffer* before) {
  require(memcmp(value,snapshot,selected->size)==0,"Partial candidate or ownership was published");
  srl_buffer after={0};require(selected->binary(value,&after)==srl_ok,"Rejection native control");
  require(before->size==after.size&&memcmp(before->data,after.data,after.size)==0,"Rejection changed native bytes");srl_buffer_free(&after);
}
/* Read independently authored XML bytes without modifying their encoding or provider chunking. */
static uint8_t* read_file(const char* path,size_t* size) {
  FILE* file=fopen(path,"rb");require(file!=NULL,"Open input");require(fseek(file,0,SEEK_END)==0,"Input seek");
  const long length=ftell(file);
  if(length<0||(uintmax_t)length>=(uintmax_t)PTRDIFF_MAX){require(false,"Input allocation extent");fclose(file);return NULL;}
  require(fseek(file,0,SEEK_SET)==0,"Input length");*size=(size_t)length;
  uint8_t* bytes=(uint8_t*)malloc(*size+1u);require(bytes!=NULL,"Input allocation");
  require(fread(bytes,1u,*size,file)==*size,"Input bytes");fclose(file);bytes[*size]=0;return bytes;
}
/* Retain the actual successfully finalized output for a separate namespace-aware XML oracle. */
static void write_file(const char* path,const srl_buffer* bytes) {
  FILE* file=fopen(path,"wb");require(file!=NULL,"Open output");require(fwrite(bytes->data,1u,bytes->size,file)==bytes->size,"Output bytes");require(fclose(file)==0,"Close output");
}
/* Require every compiler-owned initializer callback rejection to release its unpublished model. */
static void initializer_failures(const policy_case* selected) {
  ledger control;ledger_init(&control);srl_xml_operation operation=operation_for(&control);
  void* value=calloc(1u,selected->size);require(value!=NULL,"Initializer fixture");
  require(selected->init_operation(value,&operation)==srl_ok,"Operation initializer control");
  if(strcmp(selected->name,"factory")==0){require(strcmp(((operation_policy_factory_defaults*)value)->nested.label.data,"factory retained string")==0,"Authored factory result changed");}
  selected->free_value(value);
  for(size_t index=1u;index<=control.allocations;++index){
    ledger bounded;ledger_init(&bounded);bounded.deny_allocation=index;operation=operation_for(&bounded);
    require(selected->init_operation(value,&operation)!=srl_ok,"Default allocation rejection ignored");
    require(operation.caller_status==caller_marker&&operation.failure==srl_xml_operation_none,"Default caller identity/classification");
    const unsigned char* storage=(const unsigned char*)value;for(size_t byte=0;byte<selected->size;++byte){require(storage[byte]==0,"Rejected initializer retained ownership");}
  }
  free(value);
}
/* Prove local native admission remains distinct from arbitrary callback failures and semantic XML errors. */
static void local_policies(const policy_case* selected,const uint8_t* bytes,size_t size,const void* ordinary) {
  void* value=model_new(selected);void* snapshot=malloc(selected->size);require(snapshot!=NULL,"Policy snapshot");memcpy(snapshot,value,selected->size);
  srl_buffer before={0};require(selected->binary(value,&before)==srl_ok,"Policy native control");
  for(unsigned index=0;index<3u;++index){
    ledger measured;ledger_init(&measured);srl_xml_operation operation=operation_for(&measured);srl_limits limits=srl_default_limits();
    if(index==0u){limits.max_depth=0;}else if(index==1u){limits.max_elements=0;}else{limits.max_string_bytes=0;}
    require(selected->codec()->decode(value,bytes,size,&limits,NULL,&operation)!=srl_ok,"Local XML bound ignored");
    require(operation.caller_status==0&&operation.failure==srl_xml_operation_resource_limit&&operation.field&&*operation.field,"Local XML bound classification/location");
    unchanged(selected,value,snapshot,&before);
    ledger_init(&measured);operation=operation_for(&measured);srl_buffer encoded={0};
    require(selected->codec()->encode(ordinary,&encoded,&limits,NULL,&operation)!=srl_ok,"Local output XML bound ignored");
    require(operation.caller_status==0&&operation.failure==srl_xml_operation_resource_limit&&operation.field&&*operation.field,"Local output XML bound classification/location");
    require(!encoded.data&&!encoded.size,"Local output bound published bytes");srl_buffer_free(&encoded);
  }
  srl_xml_map_limits map_limits=srl_xml_default_map_limits();map_limits.max_entries=0;
  srl_xml_mixed_limits mixed_limits=srl_xml_default_mixed_limits();mixed_limits.max_fragments=0;
  srl_xml_digest_limits digest_limits=srl_xml_default_digest_limits();digest_limits.max_bytes=1;
  srl_xml_operation_policies policies={&map_limits,&mixed_limits,&digest_limits,NULL};
  if(strcmp(selected->name,"map")==0||strcmp(selected->name,"mixed")==0||strcmp(selected->name,"digest")==0){
    for(unsigned variant=0;variant<2u;++variant){
      if(variant){map_limits.max_entries=2u;map_limits.max_key_bytes=0;mixed_limits.max_fragments=10u;mixed_limits.max_text_bytes=0;}
      if(variant&&strcmp(selected->name,"digest")==0){continue;}
      for(unsigned output=0;output<2u;++output){
      ledger measured;ledger_init(&measured);srl_xml_operation operation=operation_for(&measured);operation.policies=&policies;srl_buffer encoded={0};
      const srl_status status=output?selected->codec()->encode(ordinary,&encoded,NULL,NULL,&operation):selected->codec()->decode(value,bytes,size,NULL,NULL,&operation);
      require(status!=srl_ok&&operation.caller_status==0&&operation.failure==srl_xml_operation_resource_limit,"Independent policy classification");
      require(operation.field&&*operation.field,"Independent policy lost native member");unchanged(selected,value,snapshot,&before);srl_buffer_free(&encoded);
      const char* member=strcmp(selected->name,"map")==0?".entries":strcmp(selected->name,"mixed")==0?".content":".exact";
      if(!strstr(operation.field,member)){fprintf(stderr,"case=%s direction=%u policy=%u field=%s\n",selected->name,output,variant,operation.field);}
      require(strstr(operation.field,member)!=NULL,"Independent policy names the wrong native member");
      }
    }
  }
  free(snapshot);srl_buffer_free(&before);model_delete(selected,value);
}
/* Exercise the complete actual descriptor, all measured exact boundaries and known default/factory ownership. */
static void exercise(const policy_case* selected,const uint8_t* bytes,size_t size,const char* output_path) {
  void* value=model_new(selected);ledger input;ledger_init(&input);srl_xml_operation operation=operation_for(&input);
  require(selected->codec()->decode(value,bytes,size,NULL,NULL,&operation)==srl_ok,"Operation input control");
  srl_buffer output={0},ordinary={0};ledger emitted;ledger_init(&emitted);operation=operation_for(&emitted);
  require(selected->codec()->encode(value,&output,NULL,NULL,&operation)==srl_ok,"Operation output control");
  require(selected->xml(value,&ordinary,NULL)==srl_ok&&ordinary.size==output.size&&memcmp(ordinary.data,output.data,output.size)==0,"Ordinary XML bytes changed");
  write_file(output_path,&output);initializer_failures(selected);local_policies(selected,bytes,size,value);
  for(unsigned direction=0;direction<2u;++direction){const ledger* measured=direction?&emitted:&input;
    for(size_t resource=0;resource<resource_count;++resource){if(!measured->used[resource]){continue;}
      for(unsigned tight=0;tight<2u;++tight){
        ledger bounded;ledger_init(&bounded);bounded.maximum[resource]=measured->used[resource]-(size_t)tight;operation=operation_for(&bounded);
        void* candidate=model_new(selected);void* snapshot=malloc(selected->size);require(snapshot!=NULL,"Boundary snapshot");memcpy(snapshot,candidate,selected->size);
        srl_buffer before={0},target={0};require(selected->binary(candidate,&before)==srl_ok,"Boundary native control");
        require(srl_reserve(&target,4u),"Boundary output storage");memcpy(target.data,"keep",4u);target.size=4u;const srl_buffer saved=target;
        const srl_status status=direction?selected->codec()->encode(value,&target,NULL,NULL,&operation):selected->codec()->decode(candidate,bytes,size,NULL,NULL,&operation);
        if(!tight){require(status==srl_ok,"Exact native resource boundary rejected");}
        else{require(status!=srl_ok&&operation.caller_status==caller_marker&&operation.failure==srl_xml_operation_none,"One-tight callback failure identity");
          require(operation.field&&*operation.field,"Callback resource lost native location");unchanged(selected,candidate,snapshot,&before);
          if(resource==srl_xml_operation_scalar_bytes&&strcmp(selected->name,"map")==0){
            if(strcmp(operation.field,"operation_policy::maps.entries")!=0){fprintf(stderr,"map scalar direction=%u field=%s\n",direction,operation.field);}
            require(strcmp(operation.field,"operation_policy::maps.entries")==0,"Global map scalar quota lost its native collection path");
          }
          if(direction){require(target.data==saved.data&&target.size==saved.size&&memcmp(target.data,"keep",4u)==0,"Partial output published");}}
        free(snapshot);srl_buffer_free(&before);srl_buffer_free(&target);model_delete(selected,candidate);
      }
    }
  }
  if(strcmp(selected->name,"defaults")==0||strcmp(selected->name,"factory")==0){
    ledger denied;ledger_init(&denied);denied.deny_default=strcmp(selected->name,"defaults")==0;denied.deny_factory=!denied.deny_default;operation=operation_for(&denied);
    void* candidate=model_new(selected);void* snapshot=malloc(selected->size);require(snapshot!=NULL,"Default rejection snapshot");memcpy(snapshot,candidate,selected->size);
    srl_buffer before={0};require(selected->binary(candidate,&before)==srl_ok,"Default rejection native control");
    require(selected->codec()->decode(candidate,bytes,size,NULL,NULL,&operation)!=srl_ok&&operation.caller_status==caller_marker,"Known/factory default admission bypassed");
    require(operation.field&&strstr(operation.field,denied.deny_default?"child.wireValue":"factory_child.label"),"Default native location");unchanged(selected,candidate,snapshot,&before);
    free(snapshot);srl_buffer_free(&before);model_delete(selected,candidate);
  }
  printf("{\"input\":[");for(size_t index=0;index<resource_count;++index){printf("%s%zu",index?",":"",input.used[index]);}
  printf("],\"output\":[");for(size_t index=0;index<resource_count;++index){printf("%s%zu",index?",":"",emitted.used[index]);}
  printf("],\"assertions\":%zu}\n",assertions);srl_buffer_free(&output);srl_buffer_free(&ordinary);model_delete(selected,value);
}
/* Select a typed case while keeping fixture parsing and XML counter comparison outside generated code. */
int main(int argc,char** argv) {
  require(argc==4,"Expected case, input and output paths");
  const policy_case* cases[]={&scalar_case,&map_case,&mixed_case,&digest_case,&defaults_case,&factory_case};
  const policy_case* selected=NULL;for(size_t index=0;index<sizeof(cases)/sizeof(cases[0]);++index){if(strcmp(cases[index]->name,argv[1])==0){selected=cases[index];break;}}
  require(selected!=NULL,"Unknown case");size_t size=0;uint8_t* bytes=read_file(argv[2],&size);exercise(selected,bytes,size,argv[3]);free(bytes);return EXIT_SUCCESS;
}
