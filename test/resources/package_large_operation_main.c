// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "identity.h"

static size_t passed;
static const char* directory;
static size_t owner_entries;
static const char old_bytes[]="previous complete application file";
static const serializer_package_override deflate_override[]={ {"registries",8} };

/* Keep every assertion live in optimized C11 SDK builds. */
static void require(bool value,const char* message){if(!value){fprintf(stderr,"FAILED: %s\n",message);exit(1);}++passed;}
/* Address only fixture-owned local files, never relationship target text. */
static void filename(char* result,size_t capacity,const char* name){const int count=snprintf(result,capacity,"%s/%s",directory,name);require(count>=0&&(size_t)count<capacity,"path range");}
/* Read an independently authored archive without involving generated ZIP writers. */
static srl_buffer read_file(const char* name){char path[4097];filename(path,sizeof(path),name);FILE* file=fopen(path,"rb");require(file!=NULL,"open file");require(!fseek(file,0,SEEK_END),"seek file");const long length=ftell(file);require(length>=0&&!fseek(file,0,SEEK_SET),"file length");srl_buffer result={0};require(srl_reserve(&result,(size_t)length),"file allocation");result.size=(size_t)length;require(fread(result.data,1,result.size,file)==result.size&&!fclose(file),"read file");return result;}
/* Keep a complete old application file recognizable after any pre-publication error. */
static void sentinel(void){char path[4097];filename(path,sizeof(path),"produced.opc");FILE* file=fopen(path,"wb");require(file!=NULL,"open sentinel");require(fwrite(old_bytes,1,sizeof(old_bytes),file)==sizeof(old_bytes)&&!fclose(file),"write sentinel");}
/* Confirm publication remains unchanged independently of returned error codes. */
static bool unchanged(void){srl_buffer bytes=read_file("produced.opc");const bool good=bytes.size==sizeof(old_bytes)&&!memcmp(bytes.data,old_bytes,sizeof(old_bytes));srl_buffer_free(&bytes);return good;}
/* Raise only explicit finite resource caps and select real DEFLATE for known owning roles. */
static serializer_package_budget limits(void){serializer_package_budget value=serializer_package_default_budget();value.max_part_bytes=(size_t)16<<20;value.max_archive_bytes=(size_t)128<<20;value.maximum[serializer_package_physical_bytes]=(size_t)1024<<20;value.maximum[serializer_package_expanded_bytes]=(size_t)512<<20;value.maximum[serializer_package_metadata_bytes]=(size_t)64<<20;value.maximum[serializer_package_allocation_bytes]=(size_t)2048<<20;value.maximum[serializer_package_work_units]=(size_t)8192<<20;value.maximum[serializer_package_scalar_bytes]=(size_t)256<<20;value.maximum[serializer_package_references]=4000000;value.overrides=deflate_override;value.overrides_size=sizeof(deflate_override)/sizeof(deflate_override[0]);return value;}
/* Observe a real monotonic host clock through the portable cancellation callback surface. */
static uint64_t milliseconds(void){
#if defined(_WIN32)
return GetTickCount64();
#else
struct timespec value;require(!clock_gettime(CLOCK_MONOTONIC,&value),"monotonic clock");return (uint64_t)value.tv_sec*1000u+(uint64_t)value.tv_nsec/1000000u;
#endif
}
/* Wait for the recorded host deadline: Windows tick resolution can exceed Sleep(2). */
static void expire(uint64_t deadline){while(milliseconds()<deadline){
#if defined(_WIN32)
Sleep(1);
#else
const struct timespec value={0,1000000};nanosleep(&value,NULL);
#endif
}}
typedef struct policy_state{size_t polls,stop;bool clock,armed;uint64_t deadline;} policy_state;
/* Inject cancellation or elapsed host time at measured live public operation boundaries. */
static bool policy(void* context){policy_state* state=(policy_state*)context;++state->polls;if(state->polls==state->stop){state->armed=true;if(state->clock){state->deadline=milliseconds()+1;expire(state->deadline);}}return state->armed&&(!state->clock||milliseconds()>=state->deadline);}
/* Arm policy at the last borrowed publication hook without throwing from that hook. */
static int publication(void* context,const char* boundary){policy_state* state=(policy_state*)context;if(!strcmp(boundary,"before_replace")){state->armed=true;if(state->clock){state->deadline=milliseconds()+1;expire(state->deadline);}}return 0;}
/* Propagate one caller-owned status before publication without translation. */
static int original_failure(void* context,const char* boundary){(void)context;return !strcmp(boundary,"before_replace")?-7131:0;}
/* Keep the same public ledger through decode, identities, preservation, encode/redecode and Save. */
static int operation(const srl_buffer* bytes,serializer_package_budget* budget){identity_package_archive_value value={0};int status=identity_package_archive_load(&value,bytes->data,bytes->size,serializer_package_preserve_policy(),budget);if(!status){require(value.registries_size==3,"three identity owners");for(size_t index=0;index<value.registries_size;++index){require(value.registries[index].value.entries.size==owner_entries&&value.registries[index].value.requests.size==owner_entries,"large independent key scope");}const identity_package_archive_input input=identity_package_archive_as_input(&value);char path[4097];filename(path,sizeof(path),"produced.opc");status=identity_package_archive_save(path,&input,budget,NULL,NULL);}identity_package_archive_free(&value);return status;}

/* Exercise real generated C11 owning APIs and retain exact resource/publication outcomes. */
int main(int argc,char** argv){require(argc==4,"fixture arguments");directory=argv[1];owner_entries=(size_t)strtoull(argv[2],NULL,10);const size_t expected_references=(size_t)strtoull(argv[3],NULL,10);srl_buffer valid=read_file("valid.opc"),invalid=read_file("invalid.opc");
  policy_state measured={0};serializer_package_budget baseline=limits();baseline.is_cancelled=policy;baseline.context=&measured;require(!operation(&valid,&baseline),"large combined baseline");require(baseline.used[serializer_package_references]>=expected_references*3,"shared load+Save identity/relationship accounting");
  const serializer_package_resource resources[]={serializer_package_references,serializer_package_work_units,serializer_package_allocation_bytes};
  for(size_t resource=0;resource<sizeof(resources)/sizeof(resources[0]);++resource){for(size_t tight=0;tight<2;++tight){sentinel();serializer_package_budget budget=limits();budget.maximum[resources[resource]]=baseline.used[resources[resource]]-tight;const int status=operation(&valid,&budget);require(tight?status==serializer_package_resource_limit:status==0,"exact versus one-tight cumulative resource");if(tight){require(unchanged(),"one-tight leaves old file");}}}
  serializer_package_budget budget=limits();identity_package_archive_value published={0};require(!identity_package_archive_load(&published,valid.data,valid.size,serializer_package_preserve_policy(),&budget),"load old owning result");identity_package_archive_registries_part* original=published.registries;budget=limits();require(identity_package_archive_load(&published,invalid.data,invalid.size,serializer_package_preserve_policy(),&budget)==serializer_package_codec,"invalid final-owner keyref");require(published.registries==original&&published.registries_size==3,"late identity failure retains previous aggregate");require(strstr(budget.part,"registry2.xml")!=NULL,"native failure owner context");
  const size_t boundaries[]={1,measured.polls/2,measured.polls};
  for(size_t clock=0;clock<2;++clock){for(size_t index=0;index<sizeof(boundaries)/sizeof(boundaries[0]);++index){sentinel();policy_state state={0};state.stop=boundaries[index];state.clock=clock!=0;budget=limits();budget.is_cancelled=policy;budget.context=&state;require(operation(&valid,&budget)==serializer_package_cancelled,"large callback cancellation/clock expiry status");require(unchanged(),"large interruption leaves old file");}}
  const identity_package_archive_input input=identity_package_archive_as_input(&published);char path[4097];filename(path,sizeof(path),"produced.opc");
  for(size_t clock=0;clock<2;++clock){sentinel();policy_state state={0};state.clock=clock!=0;budget=limits();budget.is_cancelled=policy;budget.context=&state;const int status=identity_package_archive_save(path,&input,&budget,publication,&state);const bool preserved=unchanged();if(status!=serializer_package_cancelled||!preserved){fprintf(stderr,"publication clock=%zu status=%d expected=%d armed=%d polls=%zu preserved=%d part=%s\n",clock,status,serializer_package_cancelled,(int)state.armed,state.polls,(int)preserved,budget.part);}require(status==serializer_package_cancelled&&preserved,"last publication callback policy fence");}
  sentinel();budget=limits();require(identity_package_archive_save(path,&input,&budget,original_failure,NULL)==-7131&&unchanged(),"original caller status and pre-publication rollback");
  budget=limits();require(!operation(&valid,&budget),"fresh-call recovery and complete Save");identity_package_archive_value reopened={0};srl_buffer saved=read_file("produced.opc");budget=limits();require(!identity_package_archive_load(&reopened,saved.data,saved.size,serializer_package_preserve_policy(),&budget)&&reopened.registries_size==3,"fresh complete output reopens");
  printf("{\"checks\":%zu,\"pipeline_references\":%zu,\"pipeline_work\":%zu,\"pipeline_allocation\":%zu,\"pipeline_polls\":%zu,\"interruptions\":8,\"deadline_surface\":\"real host monotonic predicate maps to cancelled status\"}\n",passed,baseline.used[serializer_package_references],baseline.used[serializer_package_work_units],baseline.used[serializer_package_allocation_bytes],measured.polls);
  identity_package_archive_free(&reopened);identity_package_archive_free(&published);srl_buffer_free(&saved);srl_buffer_free(&valid);srl_buffer_free(&invalid);return 0;
}
