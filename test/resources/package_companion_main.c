// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "companion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Distinguish the consumer's ownership/identity expectations from native failures. */
static void require(bool value){if(!value){fputs("Package companion expectation failed\n",stderr);exit(EXIT_FAILURE);}}
/* Read one independently authored archive without borrowing its file handle into a codec. */
static srl_buffer read_file(const char* path){FILE* file=fopen(path,"rb");require(file!=NULL&&fseek(file,0,SEEK_END)==0);long length=ftell(file);require(length>=0&&fseek(file,0,SEEK_SET)==0);srl_buffer result={0};result.size=(size_t)length;result.capacity=result.size;result.data=(uint8_t*)malloc(result.size?result.size:1);require(result.data&&fread(result.data,1,result.size,file)==result.size&&fclose(file)==0);return result;}
/* Keep QName presentation beside the original native model across package load/borrow/encode/load. */
int main(int argc,char** argv){require(argc==3);srl_buffer source=read_file(argv[1]),encoded={0};companion_graph_value value={0},copy={0};@p@_budget budget=@p@_default_budget();@p@_policy policy={0};require(companion_graph_load(&value,source.data,source.size,policy,&budget)==0);require(value.main&&value._srl_xml_main);const companion_graph_input input=companion_graph_as_input(&value);require(input.main==value.main&&input._srl_xml_main==value._srl_xml_main);budget=@p@_default_budget();require(companion_graph_encode(&input,&encoded,&budget)==0);budget=@p@_default_budget();require(companion_graph_load(&copy,encoded.data,encoded.size,policy,&budget)==0);const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){srl_buffer before={0},after={0};require(companion_outer_encode(value.main,protocols[index],&before)==srl_ok&&companion_outer_encode(copy.main,protocols[index],&after)==srl_ok);require(before.size==after.size&&!memcmp(before.data,after.data,before.size));srl_buffer_free(&before);srl_buffer_free(&after);}FILE* file=fopen(argv[2],"wb");require(file&&fwrite(encoded.data,1,encoded.size,file)==encoded.size&&fclose(file)==0);companion_graph_free(&value);companion_graph_free(&copy);srl_buffer_free(&source);srl_buffer_free(&encoded);return EXIT_SUCCESS;}
