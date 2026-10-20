# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated application references and fixed/default package envelopes with actual SDKs."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as etree
import zipfile

from package_node_dependencies import prepare_node_dependencies

ROOT = Path(__file__).resolve().parents[1]
NAMESPACE = 'urn:serializer:application-contract'
RELATIONSHIPS = 'http://schemas.openxmlformats.org/package/2006/relationships'
CONTENT_TYPES = 'http://schemas.openxmlformats.org/package/2006/content-types'


def build_fixtures(directory):
    """Author OPC independently of Serializer, including forward, duplicate and wrong-type references."""
    directory.mkdir(parents=True, exist_ok=True)
    rows = []

    def part(role, name, identity=7, *, xsi=None, root=None, compression=None):
        """Keep native model, XML identity, envelope and relationship identities independently mutable."""
        local = root or ('source' if role == 'sources' else 'other_target' if role == 'fixed' else 'target')
        attributes = {'target_id': str(identity)} if role == 'sources' else {'id': str(identity), 'name': 'seven & checked'}
        node = etree.Element('{' + NAMESPACE + '}' + local, attributes)
        if xsi is not None:
            node.set('xmlns:n', 'urn:serializer:native:contract')
            node.set('{http://www.w3.org/2001/XMLSchema-instance}type', xsi)
        return role, name, etree.tostring(node, encoding='utf-8'), compression

    def add(label, parts, expected='OK', reverse=False, ids=None):
        """Produce a complete archive through the Python standard ZIP/XML libraries."""
        manifest = etree.Element('{' + CONTENT_TYPES + '}Types')
        links = etree.Element('{' + RELATIONSHIPS + '}Relationships')
        payloads = []
        for index, (role, name, data, compression) in enumerate(parts):
            media = 'application/vnd.serializer.' + ('source' if role == 'sources' else 'target') + '+xml'
            etree.SubElement(manifest, '{' + CONTENT_TYPES + '}Override', PartName='/' + name, ContentType=media)
            etree.SubElement(links, '{' + RELATIONSHIPS + '}Relationship', Id=ids[index] if ids else 'r' + str(index),
                             Type='urn:serializer:' + role, Target=name)
            method = compression if compression is not None else zipfile.ZIP_STORED if role == 'fixed' else zipfile.ZIP_DEFLATED
            payloads.append((name, data, method))
        etree.SubElement(manifest, '{' + CONTENT_TYPES + '}Default', Extension='rels',
                         ContentType='application/vnd.openxmlformats-package.relationships+xml')
        payloads.extend((('[Content_Types].xml', etree.tostring(manifest), zipfile.ZIP_DEFLATED),
                         ('_rels/.rels', etree.tostring(links), zipfile.ZIP_DEFLATED)))
        if reverse:
            payloads.reverse()
        output = io.BytesIO()
        with zipfile.ZipFile(output, 'w') as archive:
            for name, data, method in payloads:
                archive.writestr(name, data, compress_type=method)
        (directory / (label + '.opc')).write_bytes(output.getvalue())
        rows.append((label, expected))

    valid = [part('sources', 's.xml'), part('targets', 't.xml'), part('fixed', 'fixed.xml', 9)]
    add('valid', valid)
    add('permuted', valid, reverse=True)
    add('forward-repeated', [part('sources', 's1.xml'), part('sources', 's2.xml'), part('targets', 't.xml')])
    add('zero', [part('sources', 's.xml', 0), part('targets', 't.xml', 0)])
    add('uint32-max', [part('sources', 's.xml', 4294967295), part('targets', 't.xml', 4294967295)])
    add('equivalent-type-prefix', [part('sources', 's.xml'), part('targets', 't.xml', xsi='n:target')])
    add('whitespace-xsi-type', [part('sources', 's.xml'), part('targets', 't.xml', xsi=' \t\r\nn:target \n\t')])
    add('duplicate', [part('targets', 't1.xml'), part('targets', 't2.xml')], 'REFERENCE', ids=['original-key', 'duplicate-key'])
    add('duplicate-across-types', [part('targets', 't.xml'), part('fixed', 'fixed.xml')], 'REFERENCE')
    add('missing', [part('sources', 'custom/requests.xml', 8), part('targets', 'custom/resources.xml')], 'REFERENCE', ids=['authored-request', 'authored-target'])
    add('wrong-type', [part('sources', 's.xml'), part('fixed', 'fixed.xml')], 'REFERENCE')
    add('wrong-root', [part('targets', 't.xml', root='other_target')], 'CODEC')
    add('wrong-xsi-type', [part('targets', 't.xml', xsi='n:other_target')], 'CODEC')
    add('invalid-xsi-type', [part('targets', 't.xml', xsi='n:bad:target')], 'CODEC')
    add('fixed-compression', [part('fixed', 'fixed.xml', compression=zipfile.ZIP_DEFLATED)], 'PROFILE')
    (directory / 'cases.tsv').write_text(''.join(label + '\t' + expected + '\n' for label, expected in rows), encoding='utf-8', newline='\n')
    return rows


JAVA = r'''
import java.nio.file.*;
import java.util.*;
public final class ContractMain {
  private static int passed;
  private static void check(boolean value,String message){if(!value)throw new AssertionError(message);++passed;}
  @SuppressWarnings("unchecked") private static <T extends Throwable> void raise(Throwable value)throws T{throw (T)value;}
  private static Generated.Contract.Archive.Input input(){var value=new Generated.Contract.Archive.Input();var target=new Generated.Contract.Target();target.id=7;target.name="seven & checked";var source=new Generated.Contract.Source();source.targetId=7;var fixed=new Generated.Contract.OtherTarget();fixed.id=9;fixed.name="fixed";value.targets=List.of(new Generated.SerializerPackage.ModelPart<>(target,"/t.xml","application/vnd.serializer.target+xml",0));value.sources=List.of(new Generated.SerializerPackage.ModelPart<>(source,"/s.xml","application/vnd.serializer.source+xml",0));value.fixed=fixed;return value;}
  public static void main(String[] arguments)throws Exception{
    Path directory=Path.of(arguments[0]);
    for(String row:Files.readAllLines(directory.resolve("cases.tsv"))){String[] columns=row.split("\t");boolean validated=false;try{var result=Generated.Contract.Archive.load(Files.readAllBytes(directory.resolve(columns[0]+".opc")));check(columns[1].equals("OK"),columns[0]+" accepted");check(result.value.targets.size()>0||result.value.fixed!=null,columns[0]+" value");validated=true;}catch(Generated.SerializerPackage.PackageError failure){check(!columns[1].equals("OK"),columns[0]+" "+failure);if(columns[1].equals("REFERENCE"))check(failure.code==Generated.SerializerPackage.Code.SEMANTIC&&!failure.fieldPath.isEmpty(),"reference context");if(columns[0].equals("missing"))check(failure.part.equals("/custom/requests.xml")&&failure.relationshipId.equals("authored-request"),"loaded authored request location");if(columns[0].equals("duplicate"))check(failure.part.equals("/t2.xml")&&failure.relationshipId.equals("duplicate-key"),"loaded duplicate key location");if(columns[1].equals("CODEC"))check(failure.code==Generated.SerializerPackage.Code.CODEC,"codec identity");if(columns[1].equals("PROFILE"))check(failure.code==Generated.SerializerPackage.Code.INVALID_OPTIONS,"fixed envelope");}if(!columns[1].equals("OK"))check(!validated,"partial publication");}
    var value=input();Files.write(directory.resolve("produced-default.opc"),Generated.Contract.Archive.encode(value));Files.write(directory.resolve("produced-runtime.opc"),Generated.Contract.Archive.encode(value,new Generated.SerializerPackage.Budget(),Map.of("targets","stored")));check(Generated.Contract.Archive.load(Generated.Contract.Archive.encode(value)).value.targets.get(0).value.id==7,"native roundtrip");
    for(var profiles:List.of(Map.of("fixed","deflate"),Map.of("missing","stored"),Map.of("targets","gzip"))){try{Generated.Contract.Archive.encode(value,new Generated.SerializerPackage.Budget(),profiles);throw new AssertionError("invalid override accepted");}catch(Generated.SerializerPackage.PackageError failure){check(failure.code==Generated.SerializerPackage.Code.INVALID_OPTIONS,"override code");}}
    byte[] bytes=Files.readAllBytes(directory.resolve("forward-repeated.opc"));var budget=new Generated.SerializerPackage.Budget();Generated.Contract.Archive.load(bytes,budget,null,null);long used=budget.configured().maxReferences-budget.remaining(Generated.SerializerPackage.Resource.REFERENCES);check(used==6,"three OPC links plus one key and two requests");for(boolean tight:new boolean[]{false,true}){var limits=new Generated.SerializerPackage.Limits();limits.maxReferences=used-(tight?1:0);try{Generated.Contract.Archive.load(bytes,new Generated.SerializerPackage.Budget(limits),null,null);check(!tight,"one-tight references accepted");}catch(Generated.SerializerPackage.PackageError failure){check(tight&&failure.code==Generated.SerializerPackage.Code.RESOURCE_LIMIT&&!failure.fieldPath.isEmpty()&&failure.part.equals("/s2.xml")&&failure.relationshipId.equals("r1"),"shared reference diagnostic");}}
    var physicalLimits=new Generated.SerializerPackage.Limits();physicalLimits.maxReferences=0;try{Generated.Contract.Archive.load(bytes,new Generated.SerializerPackage.Budget(physicalLimits),null,null);throw new AssertionError("physical reference bound accepted");}catch(Generated.SerializerPackage.PackageError failure){check(failure.code==Generated.SerializerPackage.Code.RESOURCE_LIMIT&&failure.part.equals("/")&&failure.fieldPath.equals("relationships")&&failure.relationshipId.equals("r0"),"physical reference diagnostic");}
    var sentinel=new IllegalStateException("caller cancellation");var limits=new Generated.SerializerPackage.Limits();limits.checkCancelled=()->{throw sentinel;};try{Generated.Contract.Archive.encode(value,new Generated.SerializerPackage.Budget(limits));throw new AssertionError("cancel accepted");}catch(IllegalStateException failure){check(failure==sentinel,"cancel identity");}
    var caller=new Generated.SerializerPackage.PackageError(Generated.SerializerPackage.Code.CODEC,"caller", "caller-part",0,"caller.field","caller-reference","caller-link");boolean[] armed={false};var callerLimits=new Generated.SerializerPackage.Limits();callerLimits.checkCancelled=()->{if(armed[0])ContractMain.<RuntimeException>raise(caller);};var composer=new Generated.SerializerPackage.Composer(new Generated.SerializerPackage.Budget(callerLimits));var role=new Generated.SerializerPackage.Role("probe","/probe.xml","application/xml","/","urn:probe","probe","stored",1,1);try{composer.single(role,7,(model,nativeLimits)->{armed[0]=true;nativeLimits.xmlOperation.consume("work",1,"nested");return new byte[0];});throw new AssertionError("caller XML policy accepted");}catch(Generated.SerializerPackage.PackageError failure){check(failure==caller&&failure.part.equals("caller-part")&&failure.fieldPath.equals("caller.field")&&failure.relationshipId.equals("caller-link"),"caller package error identity and locations");}
    for(String local:new String[]{"depth","bytes","text"}){var localComposer=new Generated.SerializerPackage.Composer(new Generated.SerializerPackage.Budget());try{localComposer.single(role,value.targets.get(0).value,(model,nativeLimits)->model.encodeXml(new Generated.Limits(local.equals("bytes")?0:16777216,local.equals("text")?0:8388608,100000,local.equals("depth")?0:128,Generated.ReadPolicy.STRICT,nativeLimits.xmlOperation),null));throw new AssertionError("native local quota accepted "+local);}catch(Generated.SerializerPackage.PackageError failure){check(failure.code==Generated.SerializerPackage.Code.RESOURCE_LIMIT&&failure.part.equals("/probe.xml")&&failure.fieldPath.equals("probe")&&failure.relationshipId.equals("probe")&&failure.getCause() instanceof IllegalArgumentException,"native local quota context "+local);}}
    byte[] foreign="<probe xmlns=\"urn:serializer:extension-quota\" xmlns:v=\"urn:vendor\" id=\"7\"><v:one/><v:two/></probe>".getBytes(java.nio.charset.StandardCharsets.UTF_8);for(int cap:new int[]{2,1}){var localComposer=new Generated.SerializerPackage.Composer(new Generated.SerializerPackage.Budget());try{localComposer.single(role,value.targets.get(0).value,(model,nativeLimits)->{var extensions=new Generated.XmlExtensionOptions(List.of(),"",new Generated.XmlExtensionLimits(128,cap,8388608));Generated.Quota.Probe.decodeXml(foreign,nativeLimits,null,new Generated.XmlMapLimits(),new Generated.XmlMixedLimits(),new Generated.XmlIdentityLimits(),new Generated.XmlDigestLimits(),extensions);throw new AssertionError("foreign occurrence accepted");});throw new AssertionError("foreign occurrence accepted");}catch(Generated.SerializerPackage.PackageError failure){check(failure.code==(cap==1?Generated.SerializerPackage.Code.RESOURCE_LIMIT:Generated.SerializerPackage.Code.CODEC)&&failure.getCause().getMessage().equals("XML extension item/occurrence limit")&&failure.part.equals("/probe.xml")&&failure.relationshipId.equals("probe"),"schema occurrence differs from configured item quota");}}
    try{value.targets.get(0).value.encodeXml(new Generated.Limits(16777216,8388608,100000,0),null);throw new AssertionError("ordinary native bound accepted");}catch(IllegalArgumentException failure){check(failure.getClass()==IllegalArgumentException.class,"ordinary native error class after package scope");}
    Path finalPath=directory.resolve("atomic.opc");Files.writeString(finalPath,"old");value.sources.get(0).value.targetId=99;try{Generated.Contract.Archive.save(finalPath,value,new Generated.SerializerPackage.Budget(),null,null);throw new AssertionError("missing ref save accepted");}catch(Generated.SerializerPackage.PackageError failure){check(Files.readString(finalPath).equals("old"),"atomic reference rejection");}
    System.out.println("{\"passed\":"+passed+",\"shared_references\":"+used+"}");
  }
}
'''

GO = r'''
package main
import("os";"path/filepath";"strings";"testing";"fmt";"errors")
func TestApplicationContract(t *testing.T){passed:=0;check:=func(value bool,message string){t.Helper();if !value{t.Fatal(message)};passed++};rows,err:=os.ReadFile("fixtures/cases.tsv");if err!=nil{t.Fatal(err)}
for _,row:=range strings.Split(strings.TrimSpace(string(rows)),"\n"){columns:=strings.Split(row,"\t");data,e:=os.ReadFile(filepath.Join("fixtures",columns[0]+".opc"));if e!=nil{t.Fatal(e)};value,e:=ContractArchiveLoad(data);if columns[1]=="OK"{check(e==nil,columns[0]+fmt.Sprint(e));check(value!=nil,"complete candidate")}else{check(value==nil&&e!=nil,"atomic "+columns[0]);var failure *SerializerPackageError;check(errors.As(e,&failure),"structured failure");if columns[1]=="REFERENCE"{check(failure.Code==SerializerPackageSemantic&&failure.FieldPath!="","reference context")};if columns[0]=="missing"{check(failure.Part=="/custom/requests.xml"&&failure.RelationshipID=="authored-request","loaded authored request location")};if columns[0]=="duplicate"{check(failure.Part=="/t2.xml"&&failure.RelationshipID=="duplicate-key","loaded duplicate location")};if columns[1]=="CODEC"{check(failure.Code==SerializerPackageCodec,"codec identity")};if columns[1]=="PROFILE"{check(failure.Code==SerializerPackageInvalidOptions,"fixed envelope")}}}
value:=&ContractArchive{Targets:[]SerializerPackageModelPart[*ContractTarget]{{Value:&ContractTarget{Id:7,Name:"seven & checked"},Name:"/t.xml",ContentType:"application/vnd.serializer.target+xml"}},Sources:[]SerializerPackageModelPart[*ContractSource]{{Value:&ContractSource{TargetId:7},Name:"/s.xml",ContentType:"application/vnd.serializer.source+xml"}},Fixed:&ContractOtherTarget{Id:9,Name:"fixed"}}
data,err:=ContractArchiveEncode(value);if err!=nil{t.Fatal(err)};if err=os.WriteFile("fixtures/produced-default.opc",data,0600);err!=nil{t.Fatal(err)};data,err=ContractArchiveEncodeWithProfiles(value,map[string]string{"targets":"stored"});if err!=nil{t.Fatal(err)};if err=os.WriteFile("fixtures/produced-runtime.opc",data,0600);err!=nil{t.Fatal(err)};decoded,err:=ContractArchiveLoad(data);check(err==nil&&decoded.Targets[0].Value.Id==7,"native roundtrip")
for _,profiles:=range []map[string]string{{"fixed":"deflate"},{"missing":"stored"},{"targets":"gzip"}}{data,err=ContractArchiveEncodeWithProfiles(value,profiles);var failure *SerializerPackageError;check(data==nil&&errors.As(err,&failure)&&failure.Code==SerializerPackageInvalidOptions,"invalid override")}
data,err=os.ReadFile("fixtures/forward-repeated.opc");if err!=nil{t.Fatal(err)};budget,err:=SerializerPackageNewBudget();if err!=nil{t.Fatal(err)};_,err=ContractArchiveLoad(data,ContractArchiveLoadOptions{Budget:budget});if err!=nil{t.Fatal(err)};used:=budget.used[SerializerPackageReferences];check(used==6,"three OPC links plus one key and two requests")
for _,tight:=range []bool{false,true}{limits:=SerializerPackageDefaultLimits();limits.MaxReferences=used;if tight{limits.MaxReferences--};b,e:=SerializerPackageNewBudget(limits);if e!=nil{t.Fatal(e)};candidate,e:=ContractArchiveLoad(data,ContractArchiveLoadOptions{Budget:b});if !tight{check(e==nil&&candidate!=nil,"exact references")}else{var failure *SerializerPackageError;check(candidate==nil&&errors.As(e,&failure)&&failure.Code==SerializerPackageResourceLimit&&failure.FieldPath!=""&&failure.Part=="/s2.xml"&&failure.RelationshipID=="r1","one-tight reference context")}}
physicalLimits:=SerializerPackageDefaultLimits();physicalLimits.MaxReferences=0;physicalBudget,e:=SerializerPackageNewBudget(physicalLimits);if e!=nil{t.Fatal(e)};candidate,e:=ContractArchiveLoad(data,ContractArchiveLoadOptions{Budget:physicalBudget});var physicalFailure *SerializerPackageError;check(candidate==nil&&errors.As(e,&physicalFailure)&&physicalFailure.Code==SerializerPackageResourceLimit&&physicalFailure.Part=="/"&&physicalFailure.FieldPath=="relationships"&&physicalFailure.RelationshipID=="r0","physical reference diagnostic")
caller:=&SerializerPackageError{Code:SerializerPackageCodec,Message:"caller",Part:"caller-part",FieldPath:"caller.field",RelationshipID:"caller-link"};armed:=false;callerLimits:=SerializerPackageDefaultLimits();callerLimits.CheckCancelled=func()error{if armed{return caller};return nil};callerBudget,e:=SerializerPackageNewBudget(callerLimits);if e!=nil{t.Fatal(e)};state:=&srlPackageState{budget:callerBudget};_,e=srlPackageEncodeNative(func(model any,options XmlOptions)([]byte,error){armed=true;return model.(*ContractTarget).EncodeXML(options)},value.Targets[0].Value,state,srlPackageRole{field:"probe"},"/probe.xml","probe");check(e==caller&&caller.Part=="caller-part"&&caller.FieldPath=="caller.field"&&caller.RelationshipID=="caller-link","caller package error identity and locations")
for _,local:=range []string{"depth","bytes","text"}{localBudget,e:=SerializerPackageNewBudget();if e!=nil{t.Fatal(e)};localComposer:=srlPackageNewComposer(localBudget);e=localComposer.models(srlPackageRole{field:"probe",name:"/probe.xml",contentType:"application/xml",source:"/",relationship:"urn:probe",id:"probe",compression:"stored",minimum:1,maximum:1},[]srlPackageModelInput{{value:value.Targets[0].Value}},func(model any,options XmlOptions)([]byte,error){switch local{case "depth":options.Limits.MaxDepth=0;case "bytes":options.Limits.MaxBytes=0;case "text":options.Limits.MaxTextBytes=0};return model.(*ContractTarget).EncodeXML(options)});var failure *SerializerPackageError;check(errors.As(e,&failure)&&failure.Code==SerializerPackageResourceLimit&&failure.Part=="/probe.xml"&&failure.FieldPath=="probe"&&failure.RelationshipID=="probe"&&failure.Cause!=nil,"native local quota context "+local+fmt.Sprint(e))}
foreign:=[]byte(`<probe xmlns="urn:serializer:extension-quota" xmlns:v="urn:vendor" id="7"><v:one/><v:two/></probe>`);for _,cap:=range []int{2,1}{localBudget,e:=SerializerPackageNewBudget();if e!=nil{t.Fatal(e)};localComposer:=srlPackageNewComposer(localBudget);e=localComposer.models(srlPackageRole{field:"probe",name:"/probe.xml",contentType:"application/xml",source:"/",relationship:"urn:probe",id:"probe",compression:"stored",minimum:1,maximum:1},[]srlPackageModelInput{{value:value.Targets[0].Value}},func(_ any,options XmlOptions)([]byte,error){configuration:=DefaultXmlExtensionOptions();configuration.Limits.MaxItems=cap;_,failure:=DecodeQuotaProbeXMLWithExtensionOptions(foreign,configuration,options);if failure==nil{return nil,errors.New("foreign occurrence accepted")};return nil,failure});var failure *SerializerPackageError;code:=SerializerPackageCodec;if cap==1{code=SerializerPackageResourceLimit};check(errors.As(e,&failure)&&failure.Code==code&&failure.Part=="/probe.xml"&&failure.RelationshipID=="probe"&&failure.Cause.Error()=="XML extension item/occurrence limit","schema occurrence differs from configured item quota"+fmt.Sprint(e))}
path:="fixtures/atomic.opc";if err=os.WriteFile(path,[]byte("old"),0600);err!=nil{t.Fatal(err)};value.Sources[0].Value.TargetId=99;err=ContractArchiveSave(path,value);old,e:=os.ReadFile(path);check(err!=nil&&e==nil&&string(old)=="old","atomic reference rejection");fmt.Printf("{\"passed\":%d,\"shared_references\":%d}\n",passed,used)
}
'''

JAVASCRIPT = r'''
import{readFile,writeFile}from'node:fs/promises';
import{ContractArchive as A,ContractTarget as Target,ContractSource as Source,ContractOtherTarget as Other,SerializerPackage as P,SerializerCompression as C,XmlLimits,XmlExtensionOptions,QuotaProbe as Probe}from'./schema.mjs';
const provider=await C.nodeProvider(),options={compressionProvider:provider};let passed=0;
function check(value,message){if(!value)throw Error(message);++passed;}
const directory=new URL('./fixtures/',import.meta.url),rows=(await readFile(new URL('cases.tsv',directory),'utf8')).trim().split('\n');
for(const row of rows){const[label,expected]=row.split('\t');let candidate=null;try{candidate=await A.load(new Uint8Array(await readFile(new URL(label+'.opc',directory))),options);check(expected==='OK',label+' accepted');check(candidate.value.targets.length||candidate.value.fixed!==null,label+' complete');}catch(error){check(expected!=='OK',label+' '+error+' '+error.cause);check(error instanceof P.Error,label+' structured');if(expected==='REFERENCE')check(error.code==='invalid_relationship'&&!!error.fieldPath,'reference context');if(label==='missing')check(error.part==='/custom/requests.xml'&&error.relationshipId==='authored-request','loaded authored request location');if(label==='duplicate')check(error.part==='/t2.xml'&&error.relationshipId==='duplicate-key','loaded duplicate location');if(expected==='CODEC')check(error.code==='invalid_metadata','codec identity');if(expected==='PROFILE')check(error.code==='invalid_options','fixed envelope');}if(expected!=='OK')check(candidate===null,'atomic publication');}
const value=new A.Input({targets:[new P.ModelPart(Object.assign(new Target(),{id:7,name:'seven & checked'}),'/t.xml','application/vnd.serializer.target+xml')],sources:[new P.ModelPart(Object.assign(new Source(),{targetId:7}),'/s.xml','application/vnd.serializer.source+xml')],fixed:Object.assign(new Other(),{id:9,name:'fixed'})});
await writeFile(new URL('produced-default.opc',directory),await A.encode(value,options));await writeFile(new URL('produced-runtime.opc',directory),await A.encode(value,{...options,profiles:{targets:'stored'}}));check((await A.load(await A.encode(value,options),options)).value.targets[0].value.id===7,'native roundtrip');
for(const profiles of[{fixed:'deflate'},{missing:'stored'},{targets:'gzip'}]){try{await A.encode(value,{...options,profiles});throw Error('override accepted');}catch(error){check(error instanceof P.Error&&error.code==='invalid_options','invalid override');}}
const bytes=new Uint8Array(await readFile(new URL('forward-repeated.opc',directory))),budget=new P.Budget();await A.load(bytes,{...options,budget});const used=budget.used.references;check(used===6,'three OPC links plus one key and two requests');for(const tight of[false,true]){try{await A.load(bytes,{...options,budget:new P.Budget(new P.Limits({maxReferences:used-(tight?1:0)}))});check(!tight,'one-tight accepted');}catch(error){check(tight&&error instanceof P.Error&&error.code==='resource_limit'&&!!error.fieldPath&&error.part==='/s2.xml'&&error.relationshipId==='r1','shared reference context');}}
try{await A.load(bytes,{...options,limits:new P.Limits({maxReferences:0})});throw Error('physical reference bound accepted');}catch(error){check(error instanceof P.Error&&error.code==='resource_limit'&&error.part==='/'&&error.fieldPath==='relationships'&&error.relationshipId==='r0','physical reference diagnostic');}
const caller=new P.Error(P.Code.InvalidOptions,'caller','caller-part','caller-link');caller.fieldPath='caller.field';let armed=false;const model=value.targets[0].value,original=model.encodeXml;model.encodeXml=function(...args){armed=true;return original.apply(this,args);};try{await A.encode(value,{...options,limits:new P.Limits({checkCancelled(){if(armed)throw caller;}})});throw Error('caller XML policy accepted');}catch(error){check(error===caller&&error.part==='caller-part'&&error.fieldPath==='caller.field'&&error.relationshipId==='caller-link','caller package error identity and locations');}finally{model.encodeXml=original;}
for(const local of['maxDepth','maxBytes','maxTextBytes']){model.encodeXml=function(limits){limits[local]=0;return original.call(this,limits);};try{await A.encode(value,options);throw Error('native local quota accepted '+local);}catch(error){check(error instanceof P.Error&&error.code==='resource_limit'&&error.part==='/t.xml'&&error.fieldPath==='targets'&&error.relationshipId==='srl_targets_0'&&!!error.cause,'native local quota context '+local+' '+error);}finally{model.encodeXml=original;}}
const foreign='<probe xmlns="urn:serializer:extension-quota" xmlns:v="urn:vendor" id="7"><v:one/><v:two/></probe>';for(const cap of[2,1]){model.encodeXml=function(limits){limits.extensions=new XmlExtensionOptions({limits:{maxItems:cap}});Probe.decodeXml(foreign,limits);throw Error('foreign occurrence accepted');};try{await A.encode(value,options);throw Error('foreign occurrence accepted');}catch(error){check(cap===1?error instanceof P.Error&&error.code==='resource_limit'&&error.part==='/t.xml'&&error.relationshipId==='srl_targets_0'&&error.cause?.message==='XML extension item/occurrence limit':error.constructor===Error&&error.message==='XML extension item/occurrence limit','schema occurrence differs from configured item quota '+error);}finally{model.encodeXml=original;}}
try{original.call(model,new XmlLimits({maxDepth:0}));throw Error('ordinary native bound accepted');}catch(error){check(error.constructor===Error,'ordinary native error class after package scope');}
const host=await P.nodeHost(),finalPath=new URL('atomic.opc',directory);await writeFile(finalPath,'old');value.sources[0].value.targetId=99;try{await A.save(finalPath.pathname.replace(/^\/([A-Za-z]:)/,'$1'),value,{...options,host});throw Error('missing ref save accepted');}catch(error){check(error instanceof P.Error&&(await readFile(finalPath,'utf8'))==='old','atomic reference rejection');}
console.log(JSON.stringify({passed,shared_references:used}));
'''


def main():
    """Generate, compile and execute actual native contracts while retaining exact tool/source evidence."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', choices=('java', 'go', 'javascript'), required=True)
    parser.add_argument('--java', default='java')
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--go', default='go')
    parser.add_argument('--node', default='node')
    parser.add_argument('--node-dependencies', type=Path)
    args = parser.parse_args()
    build, compiler = args.build.resolve(), args.compiler.resolve(strict=True)
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain below ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    started, commands = time.monotonic(), []
    rows = build_fixtures(build / 'fixtures')
    environment = os.environ.copy()
    environment.update(GOWORK='off', GOCACHE=str(build / 'gocache'), GOFLAGS='-mod=readonly')

    def run(command, label):
        """Preserve all diagnostics and never turn a failed actual SDK subprocess into a passing case."""
        argv = [str(item) for item in command]
        result = subprocess.run(argv, cwd=build, env=environment, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, encoding='utf-8', timeout=600)
        (build / (label + '.log')).write_text(result.stdout, encoding='utf-8')
        commands.append({'argv': argv, 'returncode': result.returncode})
        (build / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        if result.returncode:
            print(result.stdout, end='')
            result.check_returncode()
        return result.stdout

    schema = ROOT / 'test/resources/package_extension_quota.serializer'
    configuration = ROOT / 'test/resources/package_extension_quota.ini'
    if args.language == 'java':
        run([compiler, '--input', schema, '--config', configuration, '--language', 'java', '--output', build / 'Generated.java'], 'generation')
        (build / 'ContractMain.java').write_text(JAVA, encoding='utf-8')
        run([args.javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
             '-d', build / 'classes', build / 'Generated.java', build / 'ContractMain.java'], 'compile')
        output = run([args.java, '-cp', build / 'classes', 'ContractMain', build / 'fixtures'], 'sdk')
        tool = run([args.java, '-version'], 'version')
        sources = ('Generated.java', 'ContractMain.java')
    elif args.language == 'go':
        run([compiler, '--input', schema, '--config', configuration, '--language', 'go', '--go.output', build / 'schema.go',
             '--go.package', 'main'], 'generation')
        (build / 'go.mod').write_text('module serializer-application-contract\n\ngo 1.23\n', encoding='utf-8')
        (build / 'contract_test.go').write_text(GO, encoding='utf-8')
        output = run([args.go, 'test', '-count=1', '-v', '.'], 'sdk')
        compatibility = build / 'compatibility'
        compatibility.mkdir(exist_ok=True)
        legacy = compatibility / 'legacy.serializer'
        legacy.write_text('serializer version 1.20.0; class XmlOperationOptions { public uint32 value; }\n', encoding='utf-8')
        run([compiler, '--input', legacy, '--language', 'go', '--go.output', compatibility / 'legacy.go',
             '--go.package', 'main', '--xml.enabled', 'true'], 'compatibility-generation')
        generated = (compatibility / 'legacy.go').read_text(encoding='utf-8')
        helper = re.search(r'type (XmlOperationOptions_\d+) struct \{ Options XmlOptions;', generated).group(1)
        control = '''package main
import("testing";"errors")
// Verify old positional literals and native type identities alongside the additive context API.
func TestLegacyPositionalPolicy(t *testing.T){
  options:=XmlOptions{DefaultXmlLimits(),nil};model:=&XmlOperationOptions{Value:7};data,err:=model.EncodeXML(options);if err!=nil{t.Fatal(err)}
  calls:=0;context:=@helper@{Options:options,Operation:func(string,int64,string)error{calls++;return nil}}
  decoded,err:=DecodeXmlOperationOptionsXMLWithOperation(data,context);if err!=nil||decoded.Value!=7||calls==0{t.Fatal("additive context failed",err,calls)}
  if _,err=model.EncodeXMLWithOperation(context);err!=nil{t.Fatal(err)}
  sentinel:=errors.New("operation failure");context.Operation=func(string,int64,string)error{return sentinel};value,err:=DecodeXmlOperationOptionsXMLWithOperation(data,context);if value!=nil||err!=sentinel{t.Fatal("operation failure identity",err)}
}
'''
        (compatibility / 'compatibility_test.go').write_text(control.replace('@helper@', helper), encoding='utf-8')
        run([args.go, 'test', '-count=1', '-v', './compatibility'], 'compatibility-sdk')
        tool = run([args.go, 'version'], 'version')
        sources = ('schema.go', 'contract_test.go')
    else:
        if not args.node_dependencies:
            parser.error('Provide the repository-locked Node dependencies via --node-dependencies')
        modules = prepare_node_dependencies(build, args.node_dependencies, run)
        run([compiler, '--input', schema, '--config', configuration, '--language', 'js,typescript', '--js.output', build / 'schema.mjs',
             '--typescript.output', build / 'schema.d.mts'], 'generation')
        (build / 'main.mjs').write_text(JAVASCRIPT, encoding='utf-8')
        output = run([args.node, build / 'main.mjs'], 'sdk')
        consumer = "import{ContractArchive as A,SerializerPackage as P}from'./schema.mjs';const options:P.Options={profiles:{targets:'stored'}};void A.encode(new A.Input(),options);\n"
        (build / 'consumer.mts').write_text(consumer, encoding='utf-8')
        run([args.node, modules / 'typescript/bin/tsc', '--strict', '--noEmit', '--target', 'ES2022',
             '--module', 'NodeNext', '--moduleResolution', 'NodeNext', build / 'consumer.mts'], 'typescript')
        tool = run([args.node, '--version'], 'version')
        sources = ('schema.mjs', 'schema.d.mts', 'main.mjs', 'consumer.mts')
    decisions = [json.loads(line) for line in output.splitlines() if line.startswith('{')][-1]
    for label, expected_method in (('default', zipfile.ZIP_DEFLATED), ('runtime', zipfile.ZIP_STORED)):
        with zipfile.ZipFile(build / 'fixtures' / ('produced-' + label + '.opc')) as archive:
            assert archive.testzip() is None
            assert archive.getinfo('t.xml').compress_type == expected_method
            assert archive.getinfo('fixed.xml').compress_type == zipfile.ZIP_STORED
            target = etree.fromstring(archive.read('t.xml'))
            assert target.attrib == {'id': '7', 'name': 'seven & checked'}
    report = {'language': args.language, 'tool': tool.strip(), 'sdk': decisions,
              'independent_inputs': len(rows), 'independent_outputs': 2,
              'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
              'sources_sha256': {name: hashlib.sha256((build / name).read_bytes()).hexdigest() for name in sources},
              'elapsed_seconds': round(time.monotonic() - started, 3)}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
