// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"

static size_t checks;
static const char previous[]="previous destination";
/* Keep assertions active under optimized SDK builds. */
static void check(bool condition,const char* message){if(!condition){fprintf(stderr,"%s\n",message);exit(1);}++checks;}
/* Resolve only independently produced fixture paths. */
static void path(char* output,size_t capacity,const char* directory,const char* name,const char* suffix){const int count=snprintf(output,capacity,"%s/%s%s",directory,name,suffix);check(count>=0&&(size_t)count<capacity,"fixture path extent");}
/* Read owning fixture bytes without XML or ZIP normalization. */
static srl_buffer read_file(const char* name){FILE* file=fopen(name,"rb");check(file!=NULL,"open fixture");check(!fseek(file,0,SEEK_END),"seek fixture");const long count=ftell(file);check(count>=0&&(uintmax_t)count<(uintmax_t)PTRDIFF_MAX,"fixture extent");check(!fseek(file,0,SEEK_SET),"fixture rewind");srl_buffer bytes={0};check(srl_reserve(&bytes,(size_t)count+1u),"fixture allocation");bytes.size=(size_t)count;check(fread(bytes.data,1,bytes.size,file)==bytes.size&&!fclose(file),"fixture bytes");bytes.data[bytes.size]=0;return bytes;}
/* Publish complete consumer outputs for the independent Python observer. */
static void write_file(const char* name,const uint8_t* bytes,size_t size){FILE* file=fopen(name,"wb");check(file!=NULL,"open output");check(fwrite(bytes,1,size,file)==size&&!fclose(file),"write output");}
/* Require rejected Save to preserve the prior generation. */
static void unchanged_file(const char* name){srl_buffer bytes=read_file(name);check(bytes.size==sizeof(previous)-1u&&!memcmp(bytes.data,previous,bytes.size),"rejected Save changed destination");srl_buffer_free(&bytes);}
/* Poll cancellation through the same public ledger consumed by native codecs. */
static bool cancelled(void* context){const @p@_budget* budget=(const @p@_budget*)context;return budget->used[@p@_work_units]>0u;}
/* Exercise actual shared encode/decode allocation and traversal admission. */
static void boundaries(const pipeline_archive_input* input,const srl_buffer* source,const @p@_pipeline_override* profile){
  @p@_budget encoded=@p@_default_budget(),decoded=@p@_default_budget();srl_buffer bytes={0};pipeline_archive_value value={0};
  check(!@selected@_encode_with_profiles(input,&bytes,&encoded,profile,1u),"measure encode admission");srl_buffer_free(&bytes);
  check(!@selected@_load_with_profiles(&value,source->data,source->size,@p@_preserve_policy(),&decoded,profile,1u),"measure decode admission");pipeline_archive_free(&value);
  const @p@_resource resources[]={@p@_work_units,@p@_allocation_bytes};
  for(size_t index=0;index<sizeof(resources)/sizeof(resources[0]);++index){for(size_t tight=0;tight<2u;++tight){
    @p@_budget budget=@p@_default_budget();budget.maximum[resources[index]]=encoded.used[resources[index]]-tight;
    check(srl_reserve(&bytes,4u),"boundary output");memcpy(bytes.data,"keep",4u);bytes.size=4u;uint8_t* original=bytes.data;
    const int status=@selected@_encode_with_profiles(input,&bytes,&budget,profile,1u);check((status==0)==(tight==0),"encode exact/one-tight admission");if(tight){check(bytes.data==original&&bytes.size==4u&&!memcmp(bytes.data,"keep",4u),"encode partial publication");}srl_buffer_free(&bytes);
    budget=@p@_default_budget();budget.maximum[resources[index]]=decoded.used[resources[index]]-tight;const int loaded=@selected@_load_with_profiles(&value,source->data,source->size,@p@_preserve_policy(),&budget,profile,1u);check((loaded==0)==(tight==0),"decode exact/one-tight admission");if(tight){check(!value.main&&!value.fixed&&!value.links,"decode partial publication");}pipeline_archive_free(&value);
  }}
  @p@_budget budget=@p@_default_budget();budget.is_cancelled=cancelled;budget.context=&budget;check(@selected@_encode_with_profiles(input,&bytes,&budget,profile,1u)==@p@_cancelled&&!bytes.data,"native encode cancellation");
  budget=@p@_default_budget();budget.is_cancelled=cancelled;budget.context=&budget;check(@selected@_load_with_profiles(&value,source->data,source->size,@p@_preserve_policy(),&budget,profile,1u)==@p@_cancelled&&!value.main,"native decode cancellation");pipeline_archive_free(&value);
}
/* Consume all three real generated model envelopes and verify transactional publication. */
int main(int argc,char** argv){
  check(argc==5,"fixture arguments");const char* directory=argv[1];const char* name=argv[2];const char* codec=argv[3];const char* expected=argv[4];
  char filename[4097],saved[4097];path(filename,sizeof(filename),directory,name,".opc");path(saved,sizeof(saved),directory,name,"-saved.opc");
  srl_buffer source=read_file(filename);pipeline_archive_value value={0};@p@_budget budget=@p@_default_budget();const @p@_pipeline_override profiles[]={{"main",codec,NULL}};
  const int loaded=@selected@_load_with_profiles(&value,source.data,source.size,@p@_preserve_policy(),&budget,profiles,1u);
  if(loaded){if(strcmp(expected,"LOAD_ERROR")){fprintf(stderr,"load status=%d native=%d part=%s field=%s\n",loaded,budget.native_status,budget.part,budget.field);}check(!strcmp(expected,"LOAD_ERROR")&&!value.main&&!value.fixed&&!value.links,"unexpected or partial load failure");printf("{\"decision\":\"LOAD_ERROR\",\"checks\":%zu}\n",checks);pipeline_archive_free(&value);srl_buffer_free(&source);return 0;}
  check(!strcmp(expected,"OK"),"accepted invalid package");check(value.main&&value.fixed&&value.main->id==17u&&srl_string_equal(&value.main->label,"pipeline")&&value.main->values.size==3u&&value.main->values.data[2]==8u,"native payload");
  check(value.links_size==1u&&!value.links[0].content_type[0]&&!value.links[0].bytes.size&&!strcmp(value.links[0].name,"https://example.invalid/never-fetch?q=%2f#inert"),"inert external relationship");
  const pipeline_archive_input input=pipeline_archive_as_input(&value);const pipeline_payload* identity=input.main;srl_buffer encoded={0};budget=@p@_default_budget();const int status=@selected@_encode_with_profiles(&input,&encoded,&budget,profiles,1u);if(status){fprintf(stderr,"encode status=%d native=%d part=%s field=%s\n",status,budget.native_status,budget.part,budget.field);}check(!status&&input.main==identity,"encode native pipeline");path(filename,sizeof(filename),directory,name,"-encoded.opc");write_file(filename,encoded.data,encoded.size);
  budget=@p@_default_budget();check(!@selected@_save_with_profiles(saved,&input,&budget,NULL,NULL,profiles,1u),"Save selected native pipeline");boundaries(&input,&source,profiles);
  const @p@_pipeline_override rejected[]={{"fixed","binary_none",NULL},{"missing","xml",NULL},{"main","unknown",NULL},{"fixed",NULL,"deflate"}};
  for(size_t index=0;index<sizeof(rejected)/sizeof(rejected[0]);++index){srl_buffer output={0};budget=@p@_default_budget();check(@selected@_encode_with_profiles(&input,&output,&budget,&rejected[index],1u)!=0&&!output.data,"unknown/fixed pipeline accepted");srl_buffer_free(&output);}
  write_file(saved,(const uint8_t*)previous,sizeof(previous)-1u);check(srl_reserve(&value.links[0].bytes,1u),"external payload probe");value.links[0].bytes.data[0]=1u;value.links[0].bytes.size=1u;budget=@p@_default_budget();check(@selected@_save_with_profiles(saved,&input,&budget,NULL,NULL,profiles,1u)!=0,"external payload accepted");unchanged_file(saved);value.links[0].bytes.size=0u;budget=@p@_default_budget();check(!@selected@_save_with_profiles(saved,&input,&budget,NULL,NULL,profiles,1u),"restore Save");
  srl_buffer_free(&encoded);pipeline_archive_free(&value);srl_buffer_free(&source);printf("{\"decision\":\"OK\",\"checks\":%zu}\n",checks);return 0;
}
