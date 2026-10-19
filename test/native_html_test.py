# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated C/Rust ordinary HTML fragments with independent token/tree oracles and exact policies."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import sys

from html_interop_test import trace,tree_trace
from mobile_html_test import FIXTURES,INVALID

ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('serializer_native_html_sdk',ROOT/'example/xml/run.py')
xml_runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(xml_runner)
sdk=xml_runner.sdk

RUST=r'''mod schema;
use schema::{Html,HtmlLimits,HtmlEvent,HtmlAttribute};
use std::{env,fs};
/// Exercise ordinary HTML without using the generated XML parser or native model codecs.
fn main()->Result<(),Box<dyn std::error::Error>> {
    for path in env::args().skip(1) {
        let input=fs::read(&path)?;let valid=path.ends_with(".valid.html");
        let events=match Html::parse_fragment(&input,HtmlLimits::default()) {Ok(value)=>{assert!(valid,"Invalid HTML accepted: {}",path);value},Err(error)=>{if valid {return Err(Box::new(error));}continue;}};
        let output=Html::write_fragment(&events,HtmlLimits::default())?;
        assert_eq!(output,Html::write_fragment(&Html::parse_fragment(&output,HtmlLimits::default())?,HtmlLimits::default())?);
        fs::write(format!("{}.rust.html",path),output)?;
    }
    assert!(Html::supported_subset().contains("no doctype"));
    let input="<DIV hidden title=\"x\">é&amp;😀<br></DIV>".as_bytes();
    let events=Html::parse_fragment(input,HtmlLimits::default())?;let output=Html::write_fragment(&events,HtmlLimits::default())?;
    let exact=HtmlLimits{max_input_bytes:input.len(),max_output_bytes:output.len(),max_events:events.len(),max_depth:2,max_attributes_per_element:2,max_text_bytes:7};
    assert_eq!(output,Html::write_fragment(&Html::parse_fragment(input,exact)?,exact)?);
    for short in [HtmlLimits{max_events:events.len()-1,..exact},HtmlLimits{max_depth:1,..exact},HtmlLimits{max_attributes_per_element:1,..exact},HtmlLimits{max_text_bytes:6,..exact}] {
        assert!(Html::parse_fragment(input,short).is_err());assert!(Html::write_fragment(&events,short).is_err());
    }
    assert!(Html::parse_fragment(input,HtmlLimits{max_input_bytes:input.len()-1,..exact}).is_err());
    assert!(Html::write_fragment(&events,HtmlLimits{max_output_bytes:output.len()-1,..exact}).is_err());
    assert!(Html::parse_fragment(&[0xc3,0x28],HtmlLimits::default()).is_err());
    assert!(Html::parse_fragment(b"",HtmlLimits{max_events:1,..HtmlLimits::default()}).is_err());
    let authored=vec![HtmlEvent::StartFragment,HtmlEvent::StartElement{name:"span".into(),attributes:vec![HtmlAttribute{name:"hidden".into(),value:String::new(),boolean:true}]},
        HtmlEvent::Text("<&\r\"".into()),HtmlEvent::EndElement{name:"span".into()},HtmlEvent::EndFragment];
    assert_eq!(Html::write_fragment(&authored,HtmlLimits::default())?,b"<span hidden>&lt;&amp;&#13;\"</span>");
    assert!(Html::write_fragment(&[HtmlEvent::StartFragment,HtmlEvent::Text("\0".into()),HtmlEvent::EndFragment],HtmlLimits::default()).is_err());
    println!("Rust ordinary HTML traces, explicit subset, fatal UTF8 and exact independent policies pass");Ok(())
}
'''

C=r'''#include "schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Fail independently of the expected error path, retaining transactional destinations for inspection. */
static void require(bool condition,const char* message) {if(!condition){fprintf(stderr,"%s\n",message);exit(EXIT_FAILURE);}}
/* Read complete independent fixtures without applying XML character or namespace rules. */
static uint8_t* read_file(const char* path,size_t* size) {
  FILE* file=fopen(path,"rb");require(file!=NULL,"HTML fixture open");require(fseek(file,0,SEEK_END)==0,"HTML fixture seek");
  const long length=ftell(file);require(length>=0,"HTML fixture length");rewind(file);*size=(size_t)length;
  uint8_t* bytes=(uint8_t*)malloc(*size ? *size : 1u);require(bytes!=NULL,"HTML fixture allocation");require(fread(bytes,1,*size,file)==*size,"HTML fixture read");fclose(file);return bytes;
}
/* Reject parse failures without replacing or freeing the caller's previously initialized owning trace. */
static void rejected_parse(srl_html_events* events,const uint8_t* input,size_t size,const srl_html_limits* limits) {
  srl_html_event* original=events->data;const size_t count=events->size;
  require(srl_html_parse_fragment(events,input,size,limits)!=srl_ok,"Invalid HTML parse accepted");require(events->data==original && events->size==count,"Failed HTML parse changed destination");
}
/* Reject output failures without replacing or mutating the caller's previously initialized byte buffer. */
static void rejected_write(srl_buffer* output,const srl_html_events* events,const srl_html_limits* limits) {
  uint8_t* original=output->data;const size_t size=output->size;
  require(srl_html_write_fragment(output,events,limits)!=srl_ok,"Invalid HTML write accepted");require(output->data==original && output->size==size,"Failed HTML write changed destination");
}
/* Match independent validity decisions, owned cleanup, authored traces and all exact resource boundaries. */
int main(int argc,char** argv) {
  for(int argument=1;argument<argc;++argument) {
    const char* path=argv[argument];const size_t length=strlen(path);const bool valid=length>=11 && strcmp(path+length-11,".valid.html")==0;
    size_t size=0;uint8_t* bytes=read_file(path,&size);srl_html_events events={0};srl_buffer output={0},again={0};
    const srl_status result=srl_html_parse_fragment(&events,bytes,size,NULL);free(bytes);
    if(result!=srl_ok){require(!valid,"Valid HTML rejected");require(events.data==NULL && events.size==0,"Failed HTML candidate published");continue;}
    require(valid,"Invalid HTML accepted");require(srl_html_write_fragment(&output,&events,NULL)==srl_ok,"HTML output");
    require(srl_html_parse_fragment(&events,output.data,output.size,NULL)==srl_ok && srl_html_write_fragment(&again,&events,NULL)==srl_ok,"HTML canonical replay");
    require(output.size==again.size && (!output.size || memcmp(output.data,again.data,output.size)==0),"HTML canonical bytes changed");
    char* emitted=(char*)malloc(length+sizeof(".c.html"));require(emitted!=NULL,"HTML output path");memcpy(emitted,path,length);memcpy(emitted+length,".c.html",sizeof(".c.html"));
    FILE* file=fopen(emitted,"wb");require(file!=NULL && fwrite(output.data,1,output.size,file)==output.size,"HTML output write");fclose(file);free(emitted);
    srl_html_events_free(&events);srl_html_events_free(&events);srl_buffer_free(&output);srl_buffer_free(&again);
  }
  require(strstr(srl_html_supported_subset(),"no doctype")!=NULL,"Precise HTML subset diagnostic");
  const uint8_t input[]="<DIV hidden title=\"x\">\xc3\xa9&amp;\xf0\x9f\x98\x80<br></DIV>";
  const size_t size=sizeof(input)-1;srl_html_events events={0};srl_buffer output={0};
  require(srl_html_parse_fragment(&events,input,size,NULL)==srl_ok && srl_html_write_fragment(&output,&events,NULL)==srl_ok,"HTML exact policy preparation");
  srl_html_limits exact={size,output.size,events.size,2u,2u,7u};
  require(srl_html_parse_fragment(&events,input,size,&exact)==srl_ok && srl_html_write_fragment(&output,&events,&exact)==srl_ok,"Exact HTML resource equality");
  for(size_t bound=0;bound<4u;++bound){srl_html_limits short_limit=exact;
    if(bound==0){--short_limit.max_events;}else if(bound==1){--short_limit.max_depth;}else if(bound==2){--short_limit.max_attributes_per_element;}else{--short_limit.max_text_bytes;}
    rejected_parse(&events,input,size,&short_limit);rejected_write(&output,&events,&short_limit);
  }
  srl_html_limits short_limit=exact;--short_limit.max_input_bytes;rejected_parse(&events,input,size,&short_limit);
  short_limit=exact;--short_limit.max_output_bytes;rejected_write(&output,&events,&short_limit);
  const uint8_t malformed[]={0xc3u,0x28u};rejected_parse(&events,malformed,sizeof(malformed),NULL);
  short_limit=srl_html_default_limits();short_limit.max_events=1u;rejected_parse(&events,NULL,0,&short_limit);
  srl_html_attribute hidden={0};hidden.name="hidden";hidden.boolean=true;
  srl_html_event authored[5]={0};authored[0].kind=srl_html_start_fragment;authored[1].kind=srl_html_start_element;authored[1].name="span";authored[1].attributes=&hidden;authored[1].attribute_count=1;
  authored[2].kind=srl_html_text;authored[2].text.data="<&\r\"";authored[2].text.size=4u;authored[3].kind=srl_html_end_element;authored[3].name="span";authored[4].kind=srl_html_end_fragment;
  const srl_html_events borrowed={authored,5u};require(srl_html_write_fragment(&output,&borrowed,NULL)==srl_ok,"Authored HTML semantic events");
  const char* expected="<span hidden>&lt;&amp;&#13;\"</span>";require(output.size==strlen(expected) && memcmp(output.data,expected,output.size)==0,"Authored HTML canonical bytes");
  char invalid=0;authored[2].text.data=&invalid;authored[2].text.size=1u;rejected_write(&output,&borrowed,NULL);
  srl_html_events_free(&events);srl_buffer_free(&output);puts("C ordinary HTML traces, transactional cleanup, fatal UTF8 and exact independent policies pass");return EXIT_SUCCESS;
}
'''


def main():
    """Build actual generated SDKs, then compare every successful fragment with two independent HTML parsers."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--language',default='rust,c');parser.add_argument('--libxml2-prefix',type=Path)
    parser.add_argument('--wsl-languages',default='');parser.add_argument('--python-dependencies',type=Path)
    parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args()
    if args.python_dependencies:sys.path.insert(0,str(args.python_dependencies.resolve()))
    from lxml import html as provider
    provider.fragment_fromstring('<p>required independent tree provider</p>')
    args.compiler=args.compiler.resolve();args.build=args.build.resolve();languages=args.language.split(',')
    if not languages or len(set(languages))!=len(languages) or any(language not in ('rust','c') for language in languages):
        parser.error('Choose distinct languages from rust,c')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:parser.error('Scratch must remain under ignored out/')
    if args.libxml2_prefix:args.libxml2_prefix=args.libxml2_prefix.resolve()
    args.build.mkdir(parents=True,exist_ok=True);os.environ.setdefault('CARGO_HOME',str(args.build/'cargo-home'));os.environ['RUSTFLAGS']='-D warnings'
    paths=[]
    for index,document in enumerate(FIXTURES+INVALID):
        path=args.build/(str(index)+('.valid.html' if index<len(FIXTURES) else '.invalid.html'));path.write_bytes(document.encode());paths.append(path)
    source=args.build/'html.serializer';source.write_text('serializer version 1.20.0; namespace native_html {class sample {public uint32 value(1);}}',encoding='utf-8')
    command=[args.compiler,'--input',source,'--language',','.join(languages),'--xml.enabled','true']
    for language in languages:
        directory=args.build/language;directory.mkdir(exist_ok=True);command += ['--'+language+'.output',directory/sdk.OUTPUTS[language]]
    sdk.run(command);runner=sdk.Runner(args)
    for language in languages:
        directory=args.build/'source'/language;directory.mkdir(parents=True,exist_ok=True)
        (directory/('main.rs' if language=='rust' else 'main.c')).write_text(RUST if language=='rust' else C,encoding='utf-8')
        if language=='rust':
            for filename in ('Cargo.toml','Cargo.lock'):shutil.copyfile(ROOT/'example/xml/rust'/filename,directory/filename)
        for path in paths:Path(str(path)+'.'+language+'.html').unlink(missing_ok=True)
        launch=xml_runner.build_language(args,runner,language,args.build/language,source_root=directory.parent)
        sdk.run([*launch,*(runner.path(language,path) for path in paths)])
        for index,path in enumerate(paths):
            output=Path(str(path)+'.'+language+'.html')
            if index<len(FIXTURES):
                actual=output.read_bytes().decode()
                if trace(FIXTURES[index])!=trace(actual) or tree_trace(FIXTURES[index])!=tree_trace(actual):raise AssertionError('Independent HTML oracle differs: '+language+': '+str(index))
            elif output.exists():raise AssertionError('Invalid HTML output published: '+str(output))
    print(f'PASS: {len(paths)} HTML subset decisions, two independent semantic oracles and exact policies across {len(languages)} actual native SDKs',flush=True)


if __name__=='__main__':main()
