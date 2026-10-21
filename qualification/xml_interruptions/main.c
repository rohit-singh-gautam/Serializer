// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"
#include <stdio.h>
#include <time.h>

typedef struct context { size_t polls,threshold; clock_t deadline; } context;
enum { caller_interruption=7331 };

/* Stop on the selected poll or already expired host process-clock deadline. */
static int poll(void* userdata,const char* field) {
  context* value=(context*)userdata;(void)field;++value->polls;
  return (value->threshold&&value->polls>=value->threshold)||(!value->threshold&&clock()>=value->deadline)?caller_interruption:0;
}

/* Compare complete native bytes without aliasing owning model storage. */
static bool same(const xml_example_order* value,const srl_buffer* canonical) {
  srl_buffer encoded={0};const bool good=xml_example_order_encode(value,srl_binary_none,&encoded)==srl_ok&&encoded.size==canonical->size&&!memcmp(encoded.data,canonical->data,encoded.size);
  srl_buffer_free(&encoded);return good;
}

/* Read the already bounded maintained fixture and exercise both public codec directions. */
int main(int argc,char** argv) {
  for(int index=2;index<argc;++index) {
    FILE* file=fopen(argv[index],"rb");if(!file){return 2;}
    if(fseek(file,0,SEEK_END)){return 3;}const long length=ftell(file);if(length<0||length>16*1024*1024){return 4;}rewind(file);
    uint8_t* input=(uint8_t*)malloc((size_t)length);if(!input){return 5;}const size_t size=fread(input,1,(size_t)length,file);fclose(file);if(size!=(size_t)length){return 6;}
    xml_example_order value={0};srl_buffer canonical={0};
    if(xml_example_order_decode_xml(&value,input,size,NULL)!=srl_ok||xml_example_order_encode(&value,srl_binary_none,&canonical)!=srl_ok){return 7;}
    size_t verified=0;const size_t thresholds[]={1,1000,0};
    for(size_t writing=0;writing<2;++writing){for(size_t policy=0;policy<sizeof(thresholds)/sizeof(thresholds[0]);++policy){
      context state={0,thresholds[policy],clock()};srl_xml_operation operation={0};operation.poll=poll;operation.userdata=&state;
      srl_status status;srl_buffer candidate={0};xml_example_order decoded={0};
      const srl_xml_operation_codec* codec=srl_xml_operation_codec_xml_example_order();
      if(writing){status=codec->encode(&value,&candidate,NULL,NULL,&operation);}else{status=codec->decode(&decoded,input,size,NULL,NULL,&operation);}
      if(status==srl_ok||operation.caller_status!=caller_interruption||operation.failure!=srl_xml_operation_none||
         (state.threshold&&state.polls!=state.threshold)||candidate.size||candidate.data||decoded.id||decoded.count||decoded.customer.data||decoded.tags.data){return 8;}
      srl_buffer_free(&candidate);xml_example_order_free(&decoded);
      srl_buffer output={0};xml_example_order recovered={0};
      if(!same(&value,&canonical)||xml_example_order_encode_xml(&value,&output,NULL)!=srl_ok||xml_example_order_decode_xml(&recovered,output.data,output.size,NULL)!=srl_ok||!same(&recovered,&canonical)){return 9;}
      srl_buffer_free(&output);xml_example_order_free(&recovered);++verified;
    }}
    char path[4096];if(snprintf(path,sizeof(path),"%s/%d.wire",argv[1],index-1)<0){return 10;}file=fopen(path,"wb");if(!file){return 11;}const bool written=fwrite(canonical.data,1,canonical.size,file)==canonical.size;fclose(file);if(!written){return 12;}
    printf("{\"fixture\":%d,\"verified_interruptions\":%zu,\"recovered\":true}\n",index-1,verified);
    srl_buffer_free(&canonical);xml_example_order_free(&value);free(input);
  }
  return 0;
}
