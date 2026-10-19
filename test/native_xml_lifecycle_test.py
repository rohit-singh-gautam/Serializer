# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify C/Rust exact XML model metadata, lifecycle layouts and owning digest bytes."""
import argparse
import os
from pathlib import Path
import shutil
import struct
from lxml import etree
import native_xml_map_test as native
from javascript_xml_lifecycle_test import MODEL, NS

ROOT, xml_runner, sdk = native.ROOT, native.xml_runner, native.sdk
META = 'urn:serializer:xml:model'
MODEL=MODEL.rsplit('}',1)[0]+' class digest_holder { public digests value; } }'


def document(root, revision, payload, magic=None, attributes=''):
    """Keep model metadata separate from ordinary qualified child values."""
    return '<'+root+' xmlns="'+NS+'" xmlns:m="'+META+'"'+(' m:revision="'+revision+'"' if revision is not None else '')+(' m:magic="'+magic+'"' if magic is not None else '')+attributes+'>'+payload+'</'+root+'>'


def cases():
    """Apply independent exact integer, dotted-component and native binary32 layout boundaries."""
    result=[]
    for revision in ['9007199254740993','9007199254740994','9007199254740995']:
        field='old' if int(revision)<9007199254740994 else 'current'
        result.append(('numeric',document('numeric',revision,'<'+field+'>selected</'+field+'>','53524c',' kept="7"'),True))
    valid=result[-1][1]
    for before,after in [(' m:revision="9007199254740995"',''),(' m:magic="53524c"',''),('53524c','53524C'),('m:revision=','revision='),(' m:magic=',' m:unknown="x" m:magic='),('9007199254740995','9007199254740992'),('9007199254740995','9007199254740996'),('9007199254740995','+9007199254740995'),('9007199254740995','09007199254740995'),('<current>selected</current>','<old>selected</old>'),('<current>selected</current>','')]:
        result.append(('numeric',valid.replace(before,after),False))
    result.append(('numeric',valid+'<extra/>',False))
    for revision in ['1.2.0','1.2.9','1.3.0','1.9.65535','1.10.0']:
        field='old' if tuple(map(int,revision.split('.')))<(1,3,0) else 'current'
        result.append(('dotted',document('dotted',revision,'<'+field+'>5</'+field+'>'),True))
    for revision in ['1.11.0','1.02.0','1.2','1.2.0.0','65536.1.2',' 1.2.0']:
        result.append(('dotted',document('dotted',revision,'<old>5</old>'),False))
    for revision in ['0.5','0.74999999','0.75','1']:
        rounded=struct.unpack('f',struct.pack('f',float(revision)))[0]
        field='old' if rounded<0.75 else 'current'
        result.append(('floating',document('floating',revision,'<'+field+'>5</'+field+'>'),True))
    for revision in ['0.49','1.01','-0','.5','NaN','INF','1e999','1e-999']:
        result.append(('floating',document('floating',revision,'<old>5</old>'),False))
    result.extend([('derived',document('derived','1','<extra>present</extra>'),True),('derived',document('derived','3','<kept>7</kept><extra>present</extra>'),True),('derived',document('derived','1','<kept>7</kept><extra>present</extra>'),False),('typed',document('typed',None,'<value>9</value>','4660'),True),('typed',document('typed',None,'<value>9</value>','04660'),False)])
    for attribute in ['m:revision="1"','m:magic="x"','m:unknown="x"']:
        result.append(('plain',document('plain',None,'<value>1</value>',None,' '+attribute),False))
    result.append(('envelope','<envelope xmlns="'+NS+'"><value xmlns:m="'+META+'" m:revision="9007199254740994" m:magic="53524c" kept="7"><current>nested</current></value></envelope>',True))
    digest='<digests xmlns="'+NS+'" raw="" exact="00fF" maybe="AAbb"><algorithm>'+'01'*16+'</algorithm><items><item>AaBb</item><item>0000</item></items></digests>'
    result.append(('digests',digest,True))
    for before,after in [('00fF','00'),('00fF','00gg'),('00fF','00 fF'),('AaBb','abc'),('01'*16,'01'*15)]:
        result.append(('digests',digest.replace(before,after),False))
    return result


def check_output(root, source):
    """Use independent XML and numeric readers to inspect published selected layouts and exact bytes."""
    node=etree.fromstring(source)
    assert node.tag=='{'+NS+'}'+root
    if root=='numeric':
        revision=int(node.get('{'+META+'}revision')); assert 9007199254740993<=revision<=9007199254740995
        assert node.get('{'+META+'}magic')=='53524c'
        assert [etree.QName(child).localname for child in node]==(['old'] if revision<9007199254740994 else ['current'])
    elif root=='dotted':
        revision=tuple(map(int,node.get('{'+META+'}revision').split('.')))
        assert [etree.QName(child).localname for child in node]==(['old'] if revision<(1,3,0) else ['current'])
    elif root=='floating':
        revision=struct.unpack('f',struct.pack('f',float(node.get('{'+META+'}revision'))))[0]
        assert [etree.QName(child).localname for child in node]==(['old'] if revision<0.75 else ['current'])
    elif root=='digests':
        schema=etree.XMLSchema(etree.fromstring(('<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="'+NS+'" elementFormDefault="qualified"><xs:element name="digests"><xs:complexType><xs:sequence><xs:element name="algorithm" type="xs:hexBinary"/><xs:element name="items"><xs:complexType><xs:sequence><xs:element name="item" type="xs:hexBinary" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType></xs:element></xs:sequence><xs:attribute name="raw" type="xs:hexBinary" use="required"/><xs:attribute name="exact" type="xs:hexBinary" use="required"/><xs:attribute name="maybe" type="xs:hexBinary"/></xs:complexType></xs:element></xs:schema>').encode()))
        schema.assertValid(node); assert bytes.fromhex(node.get('exact'))==bytes([0,255]); assert node.get('exact')=='00ff'
        assert bytes.fromhex(node[0].text)==bytes([1])*16


RUST=r'''mod schema;
use schema::*;
/// Check complete owning candidates and four unchanged native protocols for every selected layout.
fn main() -> Result<(),Box<dyn std::error::Error>> {
    for path in std::env::args().skip(1) {
        let text=std::fs::read_to_string(&path)?; let valid=path.ends_with(".valid.xml");
        let kind=std::path::Path::new(&path).file_name().unwrap().to_str().unwrap().split('-').next().unwrap();
        macro_rules! probe { ($kind:ty) => { (|| -> Result<String,Error> {
            let value=<$kind>::decode_xml(&text)?; let output=value.encode_xml()?; let copy=<$kind>::decode_xml(&output)?;
            assert_eq!(output,copy.encode_xml()?);
            for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString] { assert_eq!(value.encode(protocol)?,copy.encode(protocol)?); }
            Ok(output)
        })() }; }
        let result=match kind { "numeric"=>probe!(LifecycleNumeric),"dotted"=>probe!(LifecycleDotted),"floating"=>probe!(LifecycleFloating),"derived"=>probe!(LifecycleDerived),"typed"=>probe!(LifecycleTyped),"plain"=>probe!(LifecyclePlain),"envelope"=>probe!(LifecycleEnvelope),"digests"=>probe!(LifecycleDigests),_=>panic!("Unknown fixture") };
        match result { Ok(output)=>{assert!(valid,"Invalid model published: {path}");std::fs::write(path+".rust.xml",output)?;},Err(error)=>{assert!(!valid,"Valid model rejected {path}: {error:?}");} }
    }
    let source="<digests xmlns=\"urn:serializer:js-lifecycle\" raw=\"\" exact=\"00ff\"><algorithm>00000000000000000000000000000000</algorithm><items/></digests>";
    let mut value=LifecycleDigests::decode_xml(source)?; value.raw=vec![255;1024*1024];
    let output=value.encode_xml()?; assert_eq!(LifecycleDigests::decode_xml(&output)?.raw,value.raw);
    value.raw.push(0); assert!(value.encode_xml().is_err()); assert!(LifecycleDigests::decode_xml(&output.replace("raw=\"","raw=\"00")).is_err());
    let policy=XmlDigestLimits{max_bytes:1024*1024+1}; let output=value.encode_xml_with_digest_limits(XmlLimits::default(),None,policy)?;
    assert_eq!(LifecycleDigests::decode_xml_with_digest_limits(&output,XmlLimits::default(),None,policy)?.raw,value.raw);
    value.exact.push(0); assert!(value.encode_xml_with_digest_limits(XmlLimits::default(),None,policy).is_err());
    let mut holder=LifecycleDigestHolder::default(); holder.value.raw=vec![7;17];
    let nested=holder.encode_xml()?; let policy=XmlDigestLimits{max_bytes:16};
    assert!(holder.encode_xml_with_digest_limits(XmlLimits::default(),None,policy).is_err());
    assert!(LifecycleDigestHolder::decode_xml_with_digest_limits(&nested,XmlLimits::default(),None,policy).is_err());
    holder.value.raw.pop(); assert!(holder.encode_xml_with_digest_limits(XmlLimits::default(),None,policy).is_ok());
    println!("Rust exact revisions, magic, lifecycle defaults and owning hex digests pass"); Ok(())
}
'''

C=native.C.split('/* Compare typed aliases, native wire identities and independent per-wrapper map policies. */')[0]
C_CHECK=r'''
/* Verify candidate publication, stable XML and every existing native protocol. */
static srl_status check_NAME(const uint8_t* bytes,size_t size,srl_buffer* output) {
  TYPE value={0},copy={0}; require(TYPE_init(&value)==srl_ok && TYPE_init(&copy)==srl_ok,"Initialize model");
  const TYPE original=value; srl_status status=TYPE_decode_xml(&value,bytes,size,NULL);
  if (status!=srl_ok) { require(memcmp(&value,&original,sizeof(value))==0,"Partial model published"); TYPE_free(&value); TYPE_free(&copy); return status; }
  status=TYPE_encode_xml(&value,output,NULL); require(status==srl_ok,"Encode selected model");
  srl_buffer again={0}; require(TYPE_decode_xml(&copy,output->data,output->size,NULL)==srl_ok && TYPE_encode_xml(&copy,&again,NULL)==srl_ok,"Roundtrip model");
  require(output->size==again.size && memcmp(output->data,again.data,output->size)==0,"Stable model XML");
  const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
  for (size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index) { srl_buffer first={0},second={0};
    require(TYPE_encode(&value,protocols[index],&first)==srl_ok && TYPE_encode(&copy,protocols[index],&second)==srl_ok,"Native model encode");
    require(first.size==second.size && memcmp(first.data,second.data,first.size)==0,"Native model identity"); srl_buffer_free(&first);srl_buffer_free(&second);
  } srl_buffer_free(&again);TYPE_free(&value);TYPE_free(&copy);return srl_ok;
}
'''
for kind in ['numeric','dotted','floating','derived','typed','plain','envelope','digests']:
    C+=C_CHECK.replace('NAME',kind).replace('TYPE','lifecycle_'+kind)
C+=r'''
/* Match independent validity, then exercise exact digest extent and selectable byte boundaries. */
int main(int argc,char** argv) {
  for (int index=1;index<argc;++index) {
    const char* path=argv[index];const char* name=strrchr(path,'/'); if (!name) {name=strrchr(path,'\\');} name=name?name+1:path;
    srl_buffer source={0},output={0};source.data=read_file(path,&source.size);source.capacity=source.size;srl_status status=srl_invalid;
'''
for kind in ['numeric','dotted','floating','derived','typed','plain','envelope','digests']:
    C+='    '+('if' if kind=='numeric' else 'else if')+' (strncmp(name,"'+kind+'-",'+str(len(kind)+1)+')==0) { status=check_'+kind+'(source.data,source.size,&output); }\n'
C+=r'''
    require((status==srl_ok)==(strstr(path,".valid.xml")!=NULL),path);
    if (status==srl_ok) {char target[4096];require(snprintf(target,sizeof(target),"%s.c.xml",path)>0,"Output name");FILE* file=fopen(target,"wb");require(file!=NULL,"Open output");require(fwrite(output.data,1,output.size,file)==output.size,"Write output");fclose(file);}
    srl_buffer_free(&source);srl_buffer_free(&output);
  }
  const char* source="<digests xmlns=\"urn:serializer:js-lifecycle\" raw=\"\" exact=\"00ff\"><algorithm>00000000000000000000000000000000</algorithm><items/></digests>";
  lifecycle_digests value={0},copy={0};require(lifecycle_digests_init(&value)==srl_ok && lifecycle_digests_init(&copy)==srl_ok,"Digest init");
  require(lifecycle_digests_decode_xml(&value,(const uint8_t*)source,strlen(source),NULL)==srl_ok,"Digest source");
  value.raw.data=(uint8_t*)malloc(1024u*1024u+1u);require(value.raw.data!=NULL,"Digest memory");memset(value.raw.data,255,1024u*1024u+1u);value.raw.size=1024u*1024u;value.raw.capacity=value.raw.size+1u;
  srl_buffer output={0};require(lifecycle_digests_encode_xml(&value,&output,NULL)==srl_ok && lifecycle_digests_decode_xml(&copy,output.data,output.size,NULL)==srl_ok,"Exact default digest boundary");
  require(copy.raw.size==value.raw.size && memcmp(copy.raw.data,value.raw.data,value.raw.size)==0,"Exact digest bytes");srl_buffer_free(&output);++value.raw.size;
  require(lifecycle_digests_encode_xml(&value,&output,NULL)==srl_limit && !output.data,"Default digest output limit");
  const srl_xml_digest_limits policy={1024u*1024u+1u};require(lifecycle_digests_encode_xml_with_digest_limits(&value,&output,NULL,&policy)==srl_ok,"Custom digest output bound");
  require(lifecycle_digests_decode_xml(&copy,output.data,output.size,NULL)==srl_limit,"Default digest input limit");
  require(lifecycle_digests_decode_xml_with_digest_limits(&copy,output.data,output.size,NULL,&policy)==srl_ok,"Custom digest input bound");
  value.exact.size=1; srl_buffer invalid={0};require(lifecycle_digests_encode_xml_with_digest_limits(&value,&invalid,NULL,&policy)!=srl_ok && !invalid.data,"Edited fixed digest extent");
  srl_buffer_free(&output);lifecycle_digests_free(&value);lifecycle_digests_free(&copy);
  lifecycle_digest_holder holder={0},held_copy={0};require(lifecycle_digest_holder_init(&holder)==srl_ok && lifecycle_digest_holder_init(&held_copy)==srl_ok,"Nested digest init");
  holder.value.raw.data=(uint8_t*)malloc(17u);require(holder.value.raw.data!=NULL,"Nested digest memory");memset(holder.value.raw.data,7,17u);holder.value.raw.size=holder.value.raw.capacity=17u;
  const srl_xml_digest_limits nested_policy={16u};require(lifecycle_digest_holder_encode_xml(&holder,&output,NULL)==srl_ok,"Nested digest source");
  require(lifecycle_digest_holder_decode_xml_with_digest_limits(&held_copy,output.data,output.size,NULL,&nested_policy)==srl_limit,"Nested input digest policy");srl_buffer_free(&output);
  require(lifecycle_digest_holder_encode_xml_with_digest_limits(&holder,&output,NULL,&nested_policy)==srl_limit && !output.data,"Nested output digest policy");
  --holder.value.raw.size;require(lifecycle_digest_holder_encode_xml_with_digest_limits(&holder,&output,NULL,&nested_policy)==srl_ok,"Nested exact digest boundary");srl_buffer_free(&output);lifecycle_digest_holder_free(&holder);lifecycle_digest_holder_free(&held_copy);puts("C exact model layouts, transactionality and owning digests pass");return 0;
}
'''


def main():
    """Generate through the compiler and compile strict actual native consumers for both backends."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--language',default='rust,c');parser.add_argument('--libxml2-prefix',type=Path);parser.add_argument('--wsl-languages',default='')
    parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args();args.compiler=args.compiler.resolve();args.build=args.build.resolve()
    languages=args.language.split(',')
    if len(languages)!=len(set(languages)) or any(language not in ['rust','c'] for language in languages):parser.error('Choose distinct languages rust,c')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:parser.error('Scratch must be under ignored out/')
    args.build.mkdir(parents=True,exist_ok=True)
    if args.libxml2_prefix:args.libxml2_prefix=args.libxml2_prefix.resolve()
    os.environ.setdefault('CARGO_HOME',str(args.build/'cargo-home'));os.environ['RUSTFLAGS']='-D warnings'
    (args.build/'model.serializer').write_text(MODEL,encoding='utf-8');(args.build/'options.ini').write_text('[xml]\nenabled=true\n[xml.class.lifecycle::digests]\nwrapper.items=items\nname.items=item\n',encoding='utf-8')
    paths=[]
    for index,(root,source,valid) in enumerate(cases()):
        path=args.build/f'{root}-{index}.{"valid" if valid else "invalid"}.xml';path.write_text(source,encoding='utf-8');paths.append((root,path,valid))
    command=[args.compiler,'--input',args.build/'model.serializer','--config',args.build/'options.ini','--language',','.join(languages)]
    for language in languages:
        directory=args.build/language;directory.mkdir(parents=True,exist_ok=True);command += [f'--{language}.output',directory/sdk.OUTPUTS[language]]
    sdk.run(command);runner=sdk.Runner(args)
    for language in languages:
        source=args.build/'source'/language;source.mkdir(parents=True,exist_ok=True)
        (source/('main.rs' if language=='rust' else 'main.c')).write_text(RUST if language=='rust' else C,encoding='utf-8')
        if language=='rust':
            for filename in ['Cargo.toml','Cargo.lock']:shutil.copyfile(ROOT/'example/xml/rust'/filename,source/filename)
        launch=xml_runner.build_language(args,runner,language,args.build/language,source_root=source.parent)
        for _,path,_ in paths:Path(str(path)+'.'+language+'.xml').unlink(missing_ok=True)
        sdk.run([*launch,*(runner.path(language,path) for _,path,_ in paths)])
        for root,path,valid in paths:
            output=Path(str(path)+'.'+language+'.xml')
            if valid:check_output(root,output.read_bytes())
            elif output.exists():raise AssertionError('Invalid model output published')
    print(f'PASS: {len(paths)} exact native model/digest probes and selectable boundary policies across {len(languages)} SDKs',flush=True)


if __name__=='__main__':main()
