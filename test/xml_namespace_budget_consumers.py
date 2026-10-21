# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Actual additional SDK consumers for the independent XML namespace-budget matrix."""

PYTHON = '''from pathlib import Path
import sys
from schema import Limits, QuotasSample


def main():
    """Probe fresh owning operations without retaining a failed candidate."""
    value = QuotasSample.decode_xml(Path(sys.argv[1]).read_bytes())
    output = value.encode_xml()
    Path(sys.argv[2]).write_bytes(output)
    rows = []
    for mode in ('structure', 'text'):
        for count in range(129):
            limits = Limits(**{('max_elements' if mode == 'structure' else 'max_string_bytes'): count})
            encoded, decoded = True, True
            try:
                value.encode_xml(limits)
            except (ValueError, OverflowError):
                encoded = False
            try:
                QuotasSample.decode_xml(output, limits)
            except (ValueError, OverflowError):
                decoded = False
            rows.append(f'{mode} {count} {str(encoded).lower()} {str(decoded).lower()}')
    Path(sys.argv[3]).write_text('\\n'.join(rows) + '\\n', encoding='utf-8')


if __name__ == '__main__':
    main()
'''

GO = '''package main
import("fmt";"os";"strings")
// Probe each complete operation with an independent policy value.
func main(){
 data,err:=os.ReadFile(os.Args[1]);if err!=nil{panic(err)}
 value,err:=DecodeQuotasSampleXML(data);if err!=nil{panic(err)}
 output,err:=value.EncodeXML();if err!=nil{panic(err)}
 if err=os.WriteFile(os.Args[2],output,0600);err!=nil{panic(err)}
 var rows strings.Builder
 for _,mode:=range []string{"structure","text"}{for count:=0;count<=128;count++{
   limits:=DefaultXmlLimits();if mode=="structure"{limits.MaxNodes=count;limits.MaxAttributes=count}else{limits.MaxTextBytes=count}
   _,encodeError:=value.EncodeXML(XmlOptions{limits,nil})
   _,decodeError:=DecodeQuotasSampleXML(output,XmlOptions{limits,nil})
   fmt.Fprintf(&rows,"%s %d %t %t\\n",mode,count,encodeError==nil,decodeError==nil)
 }}
 if err=os.WriteFile(os.Args[3],[]byte(rows.String()),0600);err!=nil{panic(err)}
}
'''

RUST = '''mod schema;
use schema::{QuotasSample,XmlLimits};
use std::{env,fs};
/// Qualify fresh complete messages with real generated readers and writers.
fn main()->Result<(),Box<dyn std::error::Error>>{
 let args:Vec<String>=env::args().collect();
 let value=QuotasSample::decode_xml(&fs::read_to_string(&args[1])?)?;
 let output=value.encode_xml()?;fs::write(&args[2],&output)?;
 let mut rows=String::new();
 for mode in ["structure","text"]{for count in 0..=128{
   let mut limits=XmlLimits::default();
   if mode=="structure"{limits.max_nodes=count;limits.max_attributes=count}else{limits.max_text_bytes=count}
   let encoded=value.encode_xml_with_validation(limits,None).is_ok();
   let decoded=QuotasSample::decode_xml_with_validation(&output,limits,None).is_ok();
   rows.push_str(&format!("{mode} {count} {encoded} {decoded}\\n"));
 }}
 fs::write(&args[3],rows)?;Ok(())
}
'''

JAVASCRIPT = '''import {readFileSync,writeFileSync} from 'node:fs';
import {QuotasSample,XmlLimits} from './schema.mjs';
const args=process.argv.slice(2);
const value=QuotasSample.decodeXml(readFileSync(args[0],'utf8'));
const output=value.encodeXml();writeFileSync(args[1],output);
let rows='';
for(const mode of ['structure','text']){for(let count=0;count<=128;++count){
 const limits=new XmlLimits(mode==='text'?{maxTextBytes:count}:{maxNodes:count,maxAttributes:count});
 let encoded=true,decoded=true;
 try{value.encodeXml(limits);}catch{encoded=false;}
 try{QuotasSample.decodeXml(output,limits);}catch{decoded=false;}
 rows+=`${mode} ${count} ${encoded} ${decoded}\\n`;
}}
writeFileSync(args[2],rows);
'''

C = r'''#include "schema.h"
#include <stdio.h>
#include <stdlib.h>
/* Load only the small independent fixture, retaining initialized owning values. */
int main(int argc,char** argv){
 if(argc!=4){return 2;}
 FILE* input=fopen(argv[1],"rb");if(!input){return 3;}
 unsigned char data[4096];const size_t size=fread(data,1,sizeof(data),input);fclose(input);
 quotas_sample value={0};srl_buffer output={0};
 if(quotas_sample_decode_xml(&value,data,size,NULL)!=srl_ok){return 4;}
 if(quotas_sample_encode_xml(&value,&output,NULL)!=srl_ok){return 5;}
 FILE* destination=fopen(argv[2],"wb");if(!destination){return 6;}
 const bool written=fwrite(output.data,1,output.size,destination)==output.size;fclose(destination);
 if(!written){return 7;}
 FILE* rows=fopen(argv[3],"wb");if(!rows){return 8;}
 for(size_t mode=0;mode<2;++mode){for(size_t count=0;count<=128;++count){
   srl_limits limits=srl_default_limits();if(mode==0){limits.max_elements=count;}else{limits.max_string_bytes=count;}
   srl_buffer encoded={0};quotas_sample decoded={0};
   const bool encoding=quotas_sample_encode_xml(&value,&encoded,&limits)==srl_ok;
   const bool decoding=quotas_sample_decode_xml(&decoded,output.data,output.size,&limits)==srl_ok;
   fprintf(rows,"%s %zu %s %s\n",mode==0?"structure":"text",count,encoding?"true":"false",decoding?"true":"false");
   srl_buffer_free(&encoded);quotas_sample_free(&decoded);
 }}
 fclose(rows);srl_buffer_free(&output);quotas_sample_free(&value);return 0;
}
'''

CPP = r'''#include "message.hpp"
#include <cstddef>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
// Compare fresh typed XML operations against independent limits without retaining failed output.
int main(int argc,char** argv) {
  if(argc!=4&&argc!=5){return 2;}
  namespace xml=rohit::serializer::xml;
  const std::size_t chunk_bytes=argc==5?std::stoull(argv[4]):0U;
  // Select actual retained-tree or token codecs, keeping each operation's output unpublished on failure.
  const auto encode=[chunk_bytes](const quotas::sample& value,xml::limits limits={}) {
    return chunk_bytes?xml::serialize_tokens(value,limits):xml::serialize(value,limits);
  };
  // Input token chunks deliberately include a one-byte boundary qualification mode.
  const auto decode=[chunk_bytes](std::string_view bytes,xml::limits limits={}) {
    return chunk_bytes?xml::deserialize_tokens<quotas::sample>(bytes,limits,nullptr,chunk_bytes):
        xml::deserialize<quotas::sample>(bytes,limits);
  };
  std::ifstream source{argv[1],std::ios::binary};std::ostringstream input;input<<source.rdbuf();
  const auto value=decode(input.str());
  const auto output=encode(value);
  std::ofstream destination{argv[2],std::ios::binary};destination<<output;destination.close();
  std::ofstream rows{argv[3],std::ios::binary};
  for(const auto* mode: {"structure","text","namespaces"}) {
    for(std::size_t count=0;count<=128;++count) {
      xml::limits limits{};
      if(std::string{mode}=="structure"){limits.max_nodes=count;limits.max_attributes=count;}
      else if(std::string{mode}=="text"){limits.max_text_bytes=count;}
      else{limits.max_namespace_bindings=count;}
      bool encoded=true,decoded=true;
      try{(void)encode(value,limits);}catch(const std::exception&){encoded=false;}
      try{(void)decode(output,limits);}catch(const std::exception&){decoded=false;}
      rows<<mode<<' '<<count<<' '<<(encoded?"true":"false")<<' '<<(decoded?"true":"false")<<'\n';
    }
  }
  return destination.fail()||rows.fail()?3:0;
}
'''

CONSUMERS = {
    'python': ('main.py', PYTHON),
    'go': ('main.go', GO),
    'rust': ('main.rs', RUST),
    'javascript': ('main.mjs', JAVASCRIPT),
    'typescript': ('main.mts', JAVASCRIPT),
    'c': ('main.c', C),
    'cpp': ('main.cpp', CPP),
}
