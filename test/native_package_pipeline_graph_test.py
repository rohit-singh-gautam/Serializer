# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Prove fixed native package codecs need no XML QName/foreign document companion."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile

ROOT=Path(__file__).resolve().parents[1]
MODEL='''serializer version 1.21.0;
namespace graph {
  xml(namespace("urn:graph"));
  class child { public string value xml(qname); }
  class payload { public array<child> children; }
  package archive { part main : payload { name("/main.data"); content_type("application/vnd.serializer.graph");
    relationship("urn:serializer:graph:main"); codec(CODEC); policy(fixed); } }
}
'''
CONFIG='''[xml.class.graph::payload]
extension.foreign.kind=attribute
extension.foreign.namespaces=urn:foreign
extension.foreign.policy=preserve
extension.foreign.maximum=1
'''
CONSUMER='''mod schema;
use schema::GraphArchive as A;
use schema::@p@ as p;
fn main(){let path=std::env::args().nth(1).unwrap();let bytes=std::fs::read(&path).unwrap();
 let value=A::load(&bytes,&mut p::Budget::default(),p::ExtensionPolicy::default()).unwrap();
 assert_eq!(value.main.as_ref().unwrap().children[0].value,"p:unresolved");
 let output=A::encode(&value,&mut p::Budget::default()).unwrap();
 std::fs::write(path+".output",output).unwrap();
 assert!(A::select_profiles(&[p::PipelineOverride{role:"main".into(),codec:Some(p::PayloadCodec::Xml),method:None}],&p::Budget::default()).is_err());
}
'''
C_CONSUMER='''#include "schema.h"
/* Verify native XML-annotated values without resolving names through an XML document. */
int main(int argc,char** argv){if(argc!=2)return 1;FILE* file=fopen(argv[1],"rb");if(!file)return 2;if(fseek(file,0,SEEK_END))return 3;const long length=ftell(file);if(length<0)return 4;rewind(file);srl_buffer bytes={0};if(!srl_reserve(&bytes,(size_t)length))return 5;bytes.size=(size_t)length;if(fread(bytes.data,1,bytes.size,file)!=bytes.size||fclose(file))return 6;
 graph_archive_value value={0};@p@_budget budget=@p@_default_budget();if(graph_archive_load(&value,bytes.data,bytes.size,@p@_preserve_policy(),&budget))return 7;
 if(!value.main||value.main->children.size!=1u||!srl_string_equal(&value.main->children.data[0].value,"p:unresolved"))return 8;
 const graph_archive_input input=graph_archive_as_input(&value);srl_buffer output={0};budget=@p@_default_budget();if(graph_archive_encode(&input,&output,&budget))return 9;
 char path[4097];const int count=snprintf(path,sizeof(path),"%s.output",argv[1]);if(count<0||(size_t)count>=sizeof(path))return 10;file=fopen(path,"wb");if(!file||fwrite(output.data,1,output.size,file)!=output.size||fclose(file))return 11;
 const @p@_pipeline_override override={"main","xml",NULL};srl_buffer rejected={0};budget=@p@_default_budget();if(!@selected@_encode_with_profiles(&input,&rejected,&budget,&override,1u)||rejected.data)return 12;
 srl_buffer_free(&output);srl_buffer_free(&bytes);graph_archive_free(&value);return 0;}
'''


def main():
    """Generate actual native codecs and compare native data independently of XML metadata."""
    parser=argparse.ArgumentParser()
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--cargo',default='cargo')
    parser.add_argument('--language',choices=('rust','c'),default='rust')
    parser.add_argument('--cc',default='cl' if os.name=='nt' else 'cc')
    parser.add_argument('--provider-prefix',type=Path)
    args=parser.parse_args(); build=args.build.resolve(); build.mkdir(parents=True,exist_ok=True)
    commands=[];environment=dict(os.environ)
    if args.provider_prefix:environment['PATH']=str(args.provider_prefix.resolve()/'bin')+os.pathsep+environment.get('PATH','')
    def run(command,stage):
        """Retain complete generation/compiler evidence before surfacing any failed probe."""
        result=subprocess.run(list(map(str,command)),cwd=build,capture_output=True,text=True,encoding='utf-8',env=environment)
        commands.append({'stage':stage,'argv':list(map(str,command)),'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
        (build/'commands.json').write_text(json.dumps(commands,indent=2))
        if result.returncode:
            raise RuntimeError(stage+': '+result.stdout+result.stderr)
    outputs=[]
    for codec in ('json','binary_none'):
        selected=build/codec; selected.mkdir(exist_ok=True)
        schema=selected/'model.serializer'; schema.write_text(MODEL.replace('CODEC',codec)); config=selected/'profile.ini'; config.write_text(CONFIG)
        generated=selected/('schema.rs' if args.language=='rust' else 'schema.h')
        run([args.compiler.resolve(),'--input',schema,'--config',config,'--language',args.language,'--output',generated],codec+'-generation')
        if args.language=='rust':
            helper=re.search(r'// Generated Rust package helper module: (\w+)',generated.read_text()).group(1)
            (selected/'main.rs').write_text(CONSUMER.replace('@p@',helper))
            for filename in ('Cargo.toml','Cargo.lock'):
                shutil.copyfile(ROOT/'example/xml/rust'/filename,selected/filename)
            run([args.cargo,'build','--locked','--offline','--release','--manifest-path',selected/'Cargo.toml'],codec+'-compile')
            executable=selected/'target/release'/('serializer-xml-example.exe' if os.name=='nt' else 'serializer-xml-example')
        else:
            text=generated.read_text();helper=re.search(r'/\* Generated C package prefix: (\w+) \*/',text).group(1);prefix=re.search(r'/\* Generated C selected-codec prefix: (\w+) \*/',text).group(1)
            (selected/'main.c').write_text(C_CONSUMER.replace('@p@',helper).replace('@selected@',prefix))
            executable=selected/('consumer.exe' if os.name=='nt' else 'consumer')
            if Path(args.cc).name.lower() in ('cl','cl.exe'):
                command=[args.cc,'/nologo','/std:c11','/W4','/WX','/O2','/D_CRT_SECURE_NO_WARNINGS','/DSERIALIZER_COMPRESSION_ZLIB=1',selected/'main.c','/Fe'+str(executable),'/Fo'+str(selected/'main.obj')]
                if args.provider_prefix:
                    provider=args.provider_prefix.resolve();command+=['/I'+str(provider/'include'),'/I'+str(provider/'include/libxml2'),'/link','/LIBPATH:'+str(provider/'lib')]
                command+=['libxml2.lib','z.lib']
            else:
                command=[args.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-O2','-DSERIALIZER_COMPRESSION_ZLIB=1',selected/'main.c','-o',executable]+subprocess.check_output(['pkg-config','--cflags','--libs','libxml-2.0'],text=True).split()+['-lz','-lm']
            run(command,codec+'-compile')
        lexical=b'p:unresolved'; data=json.dumps({'children':[{'value':lexical.decode()}]},separators=(',',':')).encode() if codec=='json' else b'\x01'+bytes([len(lexical)])+lexical
        ct='http://schemas.openxmlformats.org/package/2006/content-types'; rel='http://schemas.openxmlformats.org/package/2006/relationships'
        types=ET.Element('{'+ct+'}Types'); ET.SubElement(types,'{'+ct+'}Override',PartName='/main.data',ContentType='application/vnd.serializer.graph'); ET.SubElement(types,'{'+ct+'}Override',PartName='/_rels/.rels',ContentType='application/vnd.openxmlformats-package.relationships+xml')
        links=ET.Element('{'+rel+'}Relationships'); ET.SubElement(links,'{'+rel+'}Relationship',Id='main',Type='urn:serializer:graph:main',Target='main.data')
        path=selected/'input.opc'
        with zipfile.ZipFile(path,'w') as archive:
            archive.writestr('[Content_Types].xml',ET.tostring(types)); archive.writestr('_rels/.rels',ET.tostring(links)); archive.writestr('main.data',data)
        run([executable,path],codec+'-run')
        with zipfile.ZipFile(str(path)+'.output') as archive:
            actual=archive.read('main.data')
            assert (json.loads(actual)==json.loads(data)) if codec=='json' else actual==data
            assert archive.getinfo('main.data').compress_type==zipfile.ZIP_STORED
        outputs.append(str(path)+'.output')
        rejected=selected/'rejected.serializer'; rejected.write_text(MODEL.replace('CODEC',codec).replace('policy(fixed)','policy(defaults)'))
        result=subprocess.run([str(args.compiler.resolve()),'--input',str(rejected),'--config',str(config),'--language',args.language,'--output',str(selected/('rejected.rs' if args.language=='rust' else 'rejected.h'))],capture_output=True,text=True)
        assert result.returncode==0, result.stderr+result.stdout
    (build/'verification.json').write_text(json.dumps({'fixed_native_graphs':2,'xml_selectable_graphs_admitted':2,'independent_outputs':outputs},indent=2)+'\n')
    print('PASS: '+args.language+' fixed native QName/foreign graph pipelines, 2 independent outputs, XML-selectable graph availability')


if __name__=='__main__':
    main()
