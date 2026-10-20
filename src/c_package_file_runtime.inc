// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"cpackage(
#if !defined(_WIN32)
#include <stdatomic.h>
#endif
typedef int (*@p@_source)(void*,uint8_t*,size_t,size_t*);
typedef int (*@p@_sink)(void*,const uint8_t*,size_t,size_t*);
typedef int (*@p@_fault)(void*,const char*);
/* Bound private memory spooling and retain original callback error codes and ownership. */
static inline int @p@_read_source(@p@_source source,void* context,srl_buffer* result,@p@_budget* budget){if(!source){return @p@_fail(budget,@p@_invalid_options,NULL,NULL,NULL);}srl_buffer fresh={0};uint8_t bytes[@p@_chunk_bytes];while(!@p@_poll(budget)){const size_t remaining=budget->max_archive_bytes-fresh.size;const size_t capacity=remaining>=sizeof(bytes)?sizeof(bytes):remaining+1;size_t count=0;const int status=source(context,bytes,capacity,&count);if(status){@p@_fail(budget,status,NULL,NULL,NULL);break;}if(@p@_poll(budget)){break;}if(count>capacity||count>remaining){@p@_fail(budget,count>capacity?@p@_transport:@p@_resource_limit,NULL,NULL,NULL);break;}if(!count){break;}if(@p@_append(budget,&fresh,bytes,count,budget->max_archive_bytes)){break;}}if(budget->status){srl_buffer_free(&fresh);return budget->status;}srl_buffer_free(result);*result=fresh;return @p@_ok;}
/* Publish prepared bytes through a borrowed sink, handling short writes and zero-progress failures. */
static inline int @p@_write_sink(@p@_sink sink,void* context,const uint8_t* bytes,size_t size,@p@_budget* budget){if(!sink){return @p@_fail(budget,@p@_invalid_options,NULL,NULL,NULL);}size_t position=0;while(position<size&&!@p@_poll(budget)){const size_t remaining=size-position;const size_t capacity=remaining<@p@_chunk_bytes?remaining:@p@_chunk_bytes;size_t written=0;const int status=sink(context,bytes+position,capacity,&written);if(status){return @p@_fail(budget,status,NULL,NULL,NULL);}if(@p@_poll(budget)){return budget->status;}if(!written||written>capacity){return @p@_fail(budget,@p@_transport,NULL,NULL,NULL);}position+=written;}return @p@_poll(budget);}
#if defined(_WIN32)
/* Interpret public path bytes as strict UTF-8 before calling native Windows filesystem APIs. */
static inline wchar_t* @p@_wide_path(const char* path,@p@_budget* budget){const int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);if(length<=0||(size_t)length>SIZE_MAX/sizeof(wchar_t)){@p@_fail(budget,@p@_invalid_name,path,NULL,NULL);return NULL;}wchar_t* value=(wchar_t*)malloc((size_t)length*sizeof(*value));if(!value){@p@_fail(budget,@p@_allocation,path,NULL,NULL);return NULL;}if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,value,length)){free(value);@p@_fail(budget,@p@_invalid_name,path,NULL,NULL);return NULL;}return value;}
#endif
/* Allocate collision-resistant process-local stage sequence numbers without a global mutable policy. */
static inline uint_fast64_t @p@_next_sequence(void){
#if defined(_WIN32)
  static volatile LONG64 sequence=0;return (uint_fast64_t)InterlockedIncrement64(&sequence);
#else
  static atomic_uint_fast64_t sequence=0;return atomic_fetch_add_explicit(&sequence,1,memory_order_relaxed);
#endif
}
/* Create a private same-directory stage, sync it, and atomically replace only after all validation. */
static inline int @p@_save_file(const char* path,const uint8_t* bytes,size_t size,@p@_budget* budget,@p@_fault fault,void* context){if(!path||strlen(path)>@p@_name_bytes-128||@p@_poll(budget)){return @p@_fail(budget,@p@_invalid_name,path,NULL,NULL);}int status=fault?fault(context,"before_create"):0;if(status){return @p@_fail(budget,status,path,NULL,NULL);}char stage[@p@_name_bytes+1];const uint_fast64_t sequence=@p@_next_sequence();const unsigned long process=
#if defined(_WIN32)
  (unsigned long)_getpid();
#else
  (unsigned long)getpid();
#endif
  snprintf(stage,sizeof(stage),"%s.serializer-package-%lu-%" PRIuMAX "-%" PRIuFAST64 ".tmp",path,process,(uintmax_t)time(NULL),sequence);int descriptor=-1;
#if defined(_WIN32)
  wchar_t* wide_stage=@p@_wide_path(stage,budget);wchar_t* wide_path=@p@_wide_path(path,budget);if(!wide_stage||!wide_path){free(wide_stage);free(wide_path);return budget->status;}descriptor=_wopen(wide_stage,_O_BINARY|_O_WRONLY|_O_CREAT|_O_EXCL,_S_IREAD|_S_IWRITE);
#else
  descriptor=open(stage,O_WRONLY|O_CREAT|O_EXCL,S_IRUSR|S_IWUSR);
#endif
  if(descriptor<0){@p@_fail(budget,@p@_transport,path,NULL,NULL);goto finish;}size_t position=0;while(position<size&&!@p@_poll(budget)){const size_t remaining=size-position;const size_t count=remaining<@p@_chunk_bytes?remaining:@p@_chunk_bytes;
#if defined(_WIN32)
    const int written=_write(descriptor,bytes+position,(unsigned)count);
#else
    const ssize_t written=write(descriptor,bytes+position,count);
#endif
    if(written<=0){@p@_fail(budget,@p@_transport,path,NULL,NULL);break;}position+=(size_t)written;}
  if(!budget->status){
#if defined(_WIN32)
    if(_commit(descriptor)){@p@_fail(budget,@p@_transport,path,NULL,NULL);}
#else
    if(fsync(descriptor)){@p@_fail(budget,@p@_transport,path,NULL,NULL);}
#endif
  }
#if defined(_WIN32)
  if(_close(descriptor)){@p@_fail(budget,@p@_transport,path,NULL,NULL);}
#else
  if(close(descriptor)){@p@_fail(budget,@p@_transport,path,NULL,NULL);}
#endif
  descriptor=-2;if(budget->status){goto finish;}status=fault?fault(context,"after_write"):0;if(status){@p@_fail(budget,status,path,NULL,NULL);goto finish;}if(@p@_poll(budget)){goto finish;}status=fault?fault(context,"before_replace"):0;if(status){@p@_fail(budget,status,path,NULL,NULL);goto finish;}if(@p@_poll(budget)){goto finish;}
#if defined(_WIN32)
  if(!MoveFileExW(wide_stage,wide_path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){@p@_fail(budget,@p@_transport,path,NULL,NULL);goto finish;}
#else
  if(rename(stage,path)){@p@_fail(budget,@p@_transport,path,NULL,NULL);goto finish;}
#endif
  status=fault?fault(context,"after_replace"):0;if(status){@p@_fail(budget,status,path,NULL,NULL);}
finish:
  if(descriptor==-2){
#if defined(_WIN32)
    _wunlink(wide_stage);
#else
    unlink(stage);
#endif
  }
#if defined(_WIN32)
  free(wide_stage);free(wide_path);
#endif
  return budget->status;
}
)cpackage"
