# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated Python, C# and Kotlin application package contracts with independent archives."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import zipfile

import csharp_compression_test as csharp
from package_application_contract_test import ROOT, build_fixtures


CSHARP = r'''using System;
using System.IO;
using System.Collections.Generic;
using S=SerializerGenerated.Schema;
using P=SerializerGenerated.Schema.SerializerPackage;
static class Program {
  // Keep assertions active under the actual optimized SDK build.
  static void Check(bool value,string message){if(!value)throw new Exception(message);}
  // Independent native-local bounds keep ordinary exception types and become located package resources.
  static int NativeQuotas(){int count=0;var target=new S.ContractTarget{Id=7,Name="native"};var role=new P.Role("native",null,"application/xml","/","urn:serializer:native-quota",null,"stored",0,-1);var parts=new[]{new P.ModelPart<S.ContractTarget>(target,"/actual-native.xml","application/xml")};
    foreach(var local in new[]{new S.Limits(maxBytes:0),new S.Limits(maxStringBytes:0),new S.Limits(maxElements:0),new S.Limits(maxDepth:0)}){try{new P.Composer(new P.Budget()).Models(role,parts,(model,shared)=>model.EncodeXml(new S.Limits(local.MaxBytes,local.MaxStringBytes,local.MaxElements,local.MaxDepth){Operation=shared.Operation}));throw new Exception("local native quota accepted");}catch(P.PackageError error){Check(error.Code==P.Code.ResourceLimit&&error.Part=="/actual-native.xml"&&error.FieldPath.StartsWith("native",StringComparison.Ordinal)&&error.RelationshipId=="srl_native_0","local native quota classification");}++count;}
    try{target.EncodeXml(new S.Limits(maxDepth:0));throw new Exception("ordinary native quota accepted");}catch(FormatException error){Check(error.GetType()==typeof(FormatException),"ordinary native exception exact type");++count;}
    bool cancel=false;var original=new FormatException("caller quota-shaped failure");var budget=new P.Budget(new P.PackageLimits{IsCancelled=()=>cancel?throw original:false});try{new P.Composer(budget).Models(role,parts,(model,shared)=>{cancel=true;return model.EncodeXml(new S.Limits(maxDepth:0){Operation=shared.Operation});});throw new Exception("caller failure accepted");}catch(FormatException error){Check(ReferenceEquals(error,original),"caller exception identity across local bound");++count;}
    return count;}
  // Return a failing exit status without invoking a native crash-reporting dialog on Windows.
  static void Main(string[] args){try{Run(args);}catch(Exception error){Console.Error.WriteLine(error);Environment.ExitCode=1;}}
  // Exercise immutable expected types and generated automatic references on independent input.
  static void Run(string[] args){string path=args[0];int count=NativeQuotas();
    var oldRole=new P.Role("main","/main.xml","application/xml","","urn:main",null,"stored",0,1);
    var(field,name,media,parent,relationship,id,storage,minimum,maximum)=oldRole;
    Check(field=="main"&&name=="/main.xml"&&media=="application/xml"&&parent==""&&relationship=="urn:main"&&id==null&&storage=="stored"&&minimum==0&&maximum==1,"legacy role constructor/deconstruction");++count;
    // Isolate final reference resolution from key/request inventory admission.
    foreach(long work in new long[]{3,2}){var ledger=new P.Budget(new P.PackageLimits{MaxWorkUnits=work});var index=new P.ReferenceIndex(ledger);index.Add("scope",7,"target","/actual-target.xml","target.id","target-link");index.Require("scope",7,"target","/actual-request.xml","request.id","request-link");try{index.Validate();Check(work==3,"one-tight reference work accepted");}catch(P.PackageError error){Check(work==2&&error.Code==P.Code.ResourceLimit&&error.Part=="/actual-request.xml"&&error.FieldPath=="request.id"&&error.RelationshipId=="request-link","final reference work context");}++count;}
    foreach(string row in File.ReadAllLines(Path.Combine(path,"cases.tsv"))){var columns=row.Split('\t');bool accepted=false;
      Console.Error.WriteLine("Fixture "+columns[0]);
      try{S.ContractArchive.Load(File.ReadAllBytes(Path.Combine(path,columns[0]+".opc")));accepted=true;}
      catch(P.PackageError error){Check(columns[1]!="OK",columns[0]+": "+error);if(columns[1]=="REFERENCE")Check(error.Code==P.Code.InvalidReference&&error.FieldPath.Length>0&&error.Part.Length>0&&error.RelationshipId.Length>0,"reference context");if(columns[1]=="CODEC")Check(error.Code==P.Code.Codec,"codec category");if(columns[1]=="PROFILE")Check(error.Code==P.Code.InvalidState,"profile category");}
      Check(accepted==(columns[1]=="OK"),columns[0]);++count;}
    var target=new S.ContractTarget{Id=7,Name="seven & checked"};var source=new S.ContractSource{TargetId=7};
    var value=new S.ContractArchive.Input{Targets=new[]{new P.ModelPart<S.ContractTarget>(target,"/t.xml","application/vnd.serializer.target+xml")},Sources=new[]{new P.ModelPart<S.ContractSource>(source,"/s.xml","application/vnd.serializer.source+xml")},Fixed=new S.ContractOtherTarget{Id=9,Name="fixed"}};
    File.WriteAllBytes(Path.Combine(path,"produced-default.opc"),S.ContractArchive.Encode(value));value.ProfileOverrides=new Dictionary<string,string>{{"targets","stored"}};File.WriteAllBytes(Path.Combine(path,"produced-runtime.opc"),S.ContractArchive.Encode(value));
    foreach(var overrides in new[]{new Dictionary<string,string>{{"fixed","deflate"}},new Dictionary<string,string>{{"absent","stored"}},new Dictionary<string,string>{{"targets","gzip"}}}){value.ProfileOverrides=overrides;try{S.ContractArchive.Encode(value);throw new Exception("invalid profiles accepted");}catch(P.PackageError){++count;}}
    value.ProfileOverrides=null;byte[] bytes=File.ReadAllBytes(Path.Combine(path,"forward-repeated.opc"));var shared=new P.Budget();S.ContractArchive.Load(bytes,shared);long used=shared.Limits.MaxReferences-shared.Remaining(P.Resource.References);Check(used==6,"shared references");
    S.ContractArchive.Load(bytes,new P.Budget(new P.PackageLimits{MaxReferences=used}));try{S.ContractArchive.Load(bytes,new P.Budget(new P.PackageLimits{MaxReferences=used-1}));throw new Exception("one-tight accepted");}catch(P.PackageError error){Check(error.Code==P.Code.ResourceLimit&&error.FieldPath.Length>0,"reference bound context");++count;}
    string final=Path.Combine(path,"atomic.opc");File.WriteAllText(final,"old");source.TargetId=99;try{S.ContractArchive.Save(final,value);throw new Exception("bad save accepted");}catch(P.PackageError){Check(File.ReadAllText(final)=="old","atomic reference failure");++count;}
    Console.WriteLine("{\"passed\":"+count+",\"references\":"+used+"}");
  }
}
'''

KOTLIN = r'''package contract
import java.nio.file.Files
import java.nio.file.Path
// Keep assertions active in optimized JVM execution.
private fun checkValue(value: Boolean,message: String){if(!value)error(message)}
// Exercise real generated native-local bounds without changing ordinary XML failures or caller identity.
private fun nativeQuotas(): Int {var count=0;val target=ContractTarget().also{it.id=7u;it.name="native"};val role=SerializerPackage.Role("native",null,"application/xml","/","urn:serializer:native-quota",null,"stored",0,-1);val parts=listOf(SerializerPackage.ModelPart(target,"/actual-native.xml","application/xml"))
    for(local in listOf(XmlLimits(maxBytes=0),XmlLimits(maxTextBytes=0),XmlLimits(maxNodes=0),XmlLimits(maxDepth=0))){try{SerializerPackage.Composer().models(role,parts){model,context->model.encodeXml(local.copy().also{it.srlPackageContext=SrlXmlBudget.Operation(context::check,context::consume,context::resourceLimit)})};error("local native quota accepted")}catch(failure:SerializerPackage.PackageError){checkValue(failure.code==SerializerPackage.Code.RESOURCE_LIMIT&&failure.part=="/actual-native.xml"&&failure.fieldPath.startsWith("native")&&failure.relationshipId=="srl_native_0","local native quota classification")};++count}
    try{target.encodeXml(XmlLimits(maxDepth=0));error("ordinary native quota accepted")}catch(failure:IllegalArgumentException){checkValue(failure.javaClass==IllegalArgumentException::class.java,"ordinary native exception exact type");++count}
    var cancel=false;val original=IllegalArgumentException("caller quota-shaped failure");val budget=SerializerPackage.Budget(SerializerPackage.Limits(checkCancelled={if(cancel)throw original}));try{SerializerPackage.Composer(budget).models(role,parts){model,context->cancel=true;model.encodeXml(XmlLimits(maxDepth=0).also{it.srlPackageContext=SrlXmlBudget.Operation(context::check,context::consume,context::resourceLimit)})};error("caller failure accepted")}catch(failure:IllegalArgumentException){checkValue(failure===original,"caller exception identity across local bound");++count}
    return count}
// Verify independent envelopes before claiming generated profile/reference acceptance.
fun main(args: Array<String>){val path=Path.of(args[0]);var count=nativeQuotas()
    // Preserve both historical operation constructor forms when adding a package-local failure hook.
    var oldAdmissions=0
    SrlXmlBudget.Operation({}) { kind,amount,field -> checkValue(kind=="work"&&amount==2L&&field=="old","legacy trailing operation");++oldAdmissions }.charge("work",2,"old")
    SrlXmlBudget.Operation({}, { kind,amount,field -> checkValue(kind=="work"&&amount==3L&&field=="positional","legacy positional operation");++oldAdmissions }).charge("work",3,"positional")
    checkValue(oldAdmissions==2,"legacy operation admissions");++count
    // Preserve source and Java constructor/copy signatures from the nine-property role API.
    val legacyRole=SerializerPackage.Role("main","/main.xml","application/xml","","urn:main",null,"stored",0,1)
    val(field,name,media,parent,relationship,id,storage,minimum,maximum)=legacyRole
    checkValue(field=="main"&&name=="/main.xml"&&media=="application/xml"&&parent==""&&relationship=="urn:main"&&id==null&&storage=="stored"&&minimum==0L&&maximum==1L,"legacy role components")
    val legacyTypes=arrayOf(String::class.java,String::class.java,String::class.java,String::class.java,String::class.java,String::class.java,String::class.java,java.lang.Long.TYPE,java.lang.Long.TYPE)
    SerializerPackage.Role::class.java.getConstructor(*legacyTypes);SerializerPackage.Role::class.java.getMethod("copy",*legacyTypes)
    val oldCopyDefault=SerializerPackage.Role::class.java.getDeclaredMethod("copy\$default",SerializerPackage.Role::class.java,*legacyTypes,java.lang.Integer.TYPE,Any::class.java)
    val profiled=SerializerPackage.Role("main","/main.xml","application/xml","/","urn:main",null,"stored",0,1,"fixed","{urn:root}main","{urn:type}main")
    val copied=oldCopyDefault.invoke(null,profiled,null,null,null,null,null,null,"deflate",0L,0L,447,null) as SerializerPackage.Role
    checkValue(copied.compression=="deflate"&&copied.policy==profiled.policy&&copied.expectedRoot==profiled.expectedRoot&&copied.expectedType==profiled.expectedType&&copied.schemaRole==profiled.schemaRole,"legacy copy default preserves profile metadata")
    checkValue(legacyRole.copy(field,name,media,parent,relationship,id,storage,minimum,maximum)==legacyRole,"legacy role copy");++count
    // Keep final reference work located after inventory collection has completed.
    for(work in listOf(3L,2L)){val ledger=SerializerPackage.Budget(SerializerPackage.Limits(maxWorkUnits=work));val index=SerializerPackage.ReferenceIndex(ledger);index.add("scope",7,"target","/actual-target.xml","target.id","target-link");index.require("scope",7,"target","/actual-request.xml","request.id","request-link");try{index.validate();checkValue(work==3L,"one-tight reference work accepted")}catch(failure:SerializerPackage.PackageError){checkValue(work==2L&&failure.code==SerializerPackage.Code.RESOURCE_LIMIT&&failure.part=="/actual-request.xml"&&failure.fieldPath=="request.id"&&failure.relationshipId=="request-link","final reference work context")};++count}
    for(row in Files.readAllLines(path.resolve("cases.tsv"))){val columns=row.split('\t');var accepted=false
        try{ContractArchive.load(Files.readAllBytes(path.resolve(columns[0]+".opc")));accepted=true}
        catch(failure:SerializerPackage.PackageError){checkValue(columns[1]!="OK",columns[0]+": "+failure);if(columns[1]=="REFERENCE")checkValue(failure.code==SerializerPackage.Code.INVALID_REFERENCE&&failure.fieldPath.isNotEmpty()&&failure.part.isNotEmpty()&&failure.relationshipId.isNotEmpty(),"reference context");if(columns[1]=="CODEC")checkValue(failure.code==SerializerPackage.Code.CODEC,"codec category");if(columns[1]=="PROFILE")checkValue(failure.code==SerializerPackage.Code.INVALID_OPTIONS,"profile category")}
        checkValue(accepted==(columns[1]=="OK"),columns[0]);++count}
    val target=ContractTarget().also{it.id=7u;it.name="seven & checked"};val source=ContractSource().also{it.targetId=7u}
    val value=ContractArchive.Input().also{it.targets=listOf(SerializerPackage.ModelPart(target,"/t.xml","application/vnd.serializer.target+xml"));it.sources=listOf(SerializerPackage.ModelPart(source,"/s.xml","application/vnd.serializer.source+xml"));it.fixed=ContractOtherTarget().also{model->model.id=9u;model.name="fixed"}}
    Files.write(path.resolve("produced-default.opc"),ContractArchive.encode(value));value.profileOverrides=mapOf("targets" to "stored");Files.write(path.resolve("produced-runtime.opc"),ContractArchive.encode(value))
    for(profiles in listOf(mapOf("fixed" to "deflate"),mapOf("absent" to "stored"),mapOf("targets" to "gzip"))){value.profileOverrides=profiles;try{ContractArchive.encode(value);error("invalid profiles accepted")}catch(failure:SerializerPackage.PackageError){++count}}
    value.profileOverrides=emptyMap();val bytes=Files.readAllBytes(path.resolve("forward-repeated.opc"));val shared=SerializerPackage.Budget();ContractArchive.load(bytes,shared);val used=shared.configured().maxReferences-shared.remaining(SerializerPackage.Resource.REFERENCES);checkValue(used==6L,"shared references")
    ContractArchive.load(bytes,SerializerPackage.Budget(SerializerPackage.Limits(maxReferences=used)));try{ContractArchive.load(bytes,SerializerPackage.Budget(SerializerPackage.Limits(maxReferences=used-1)));error("one-tight accepted")}catch(failure:SerializerPackage.PackageError){checkValue(failure.code==SerializerPackage.Code.RESOURCE_LIMIT&&failure.fieldPath.isNotEmpty(),"reference bound context");++count}
    val final=path.resolve("atomic.opc");Files.writeString(final,"old");source.targetId=99u;try{ContractArchive.save(final,value);error("bad save accepted")}catch(failure:SerializerPackage.PackageError){checkValue(Files.readString(final)=="old","atomic reference failure");++count}
    println("{\"passed\":$count,\"references\":$used}")
}
'''


def python_checks(module, directory, rows):
    """Execute the generated module without substituting or patching its prepared source."""
    archive = module.ContractArchive
    count = 0
    for label, expected in rows:
        try:
            archive.load((directory / (label + '.opc')).read_bytes())
            assert expected == 'OK', label
        except module.PackageError as error:
            assert expected != 'OK', (label, str(error))
            if expected == 'REFERENCE':
                assert error.code == 'invalid_reference' and error.field_path and error.part and error.relationship_id, (label, error.code)
            elif expected == 'PROFILE':
                assert error.code == 'invalid_state', (label, error.code)
            else:
                assert error.code == 'codec', (label, error.code)
        count += 1
    target, source, fixed = module.ContractTarget(), module.ContractSource(), module.ContractOtherTarget()
    target.id, target.name, source.target_id = 7, 'seven & checked', 7
    fixed.id, fixed.name = 9, 'fixed'
    value = archive.Input(targets=[module.ModelPart(target, '/t.xml', 'application/vnd.serializer.target+xml')],
                          sources=[module.ModelPart(source, '/s.xml', 'application/vnd.serializer.source+xml')], fixed=fixed)
    (directory / 'produced-default.opc').write_bytes(archive.encode(value))
    value.profiles = {'targets': 'stored'}
    (directory / 'produced-runtime.opc').write_bytes(archive.encode(value))
    for profiles in ({'fixed': 'deflate'}, {'absent': 'stored'}, {'targets': 'gzip'}):
        value.profiles = profiles
        try:
            archive.encode(value)
            raise AssertionError('Invalid profiles accepted')
        except module.PackageError:
            count += 1
    value.profiles = None
    # Root preflight is codec work and must preserve the selected authored part/link too.
    target_role = next(role for role in archive._roles if role['role'] == 'targets')
    for resource in ('metadata_bytes', 'work_units'):
        shared = module.PackageBudget(module.PackageLimits(**{'max_' + resource: 0}))
        try:
            module._pkg_dispatch(target.encode_xml(), target_role, shared, '/custom/resources.xml', 'authored-target')
            raise AssertionError('Root preflight admission accepted')
        except module.PackageError as error:
            assert (error.code, error.part, error.field_path, error.relationship_id) == (
                'resource_limit', '/custom/resources.xml', 'targets', 'authored-target')
        count += 1
    # Final forward resolution must retain its source after selector admission has finished.
    for work_limit in (0, 1):
        shared = module.PackageBudget(module.PackageLimits(max_work_units=work_limit))
        try:
            module._pkg_validate_references(value, archive._roles, shared)
            assert work_limit == 1, 'One-tight final reference work accepted'
        except module.PackageError as error:
            assert work_limit == 0
            assert (error.code, error.part, error.field_path, error.relationship_id) == (
                'resource_limit', '/s.xml', 'sources.target_id', 'srl_sources_0')
        count += 1
    data = (directory / 'forward-repeated.opc').read_bytes()
    shared = module.PackageBudget()
    archive.load(data, budget=shared)
    used = shared.used['references']
    assert used == 6
    archive.load(data, limits=module.PackageLimits(max_references=used))
    try:
        archive.load(data, limits=module.PackageLimits(max_references=used - 1))
        raise AssertionError('One-tight admission accepted')
    except module.PackageError as error:
        assert error.code == 'resource_limit' and error.field_path
        count += 1
    final = directory / 'atomic.opc'
    final.write_bytes(b'old')
    source.target_id = 99
    try:
        archive.save(final, value)
        raise AssertionError('Invalid Save accepted')
    except module.PackageError:
        assert final.read_bytes() == b'old'
        count += 1
    return {'passed': count, 'references': used}


def main():
    """Generate with the selected compiler and retain actual SDK logs and independent output checks."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', choices=('python', 'csharp', 'kotlin'), required=True)
    parser.add_argument('--dotnet', default=shutil.which('dotnet'))
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--kotlinc', default=os.environ.get('SERIALIZER_KOTLINC') or shutil.which('kotlinc') or shutil.which('kotlinc.bat'))
    args = parser.parse_args()
    build, compiler = args.build.resolve(), args.compiler.resolve(strict=True)
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must stay below ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    (build / 'verification.json').unlink(missing_ok=True)
    directory = build / 'fixtures'
    rows = build_fixtures(directory)
    environment, commands = os.environ.copy(), []
    started = time.monotonic()

    def run(command, label):
        """Retain every exact tool invocation and fail on the real SDK exit status."""
        command = [str(item) for item in command]
        log = build / (label + '.log')
        with log.open('w', encoding='utf-8') as output:
            result = subprocess.run(command, env=environment, cwd=build, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=180)
        output = log.read_text(encoding='utf-8')
        commands.append({'argv': command, 'returncode': result.returncode})
        if result.returncode:
            print(output, end='')
            result.check_returncode()
        return output

    schema = ROOT / 'test/resources/package_application_contract.serializer'
    if args.language == 'python':
        generated = build / 'schema.py'
        run([compiler, '--input', schema, '--language', 'python', '--python.output', generated], 'generation')
        spec = importlib.util.spec_from_file_location('package_contract_generated', generated)
        module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = module
        spec.loader.exec_module(module)
        counts, tool = python_checks(module, directory, rows), os.sys.version
    elif args.language == 'csharp':
        if not args.dotnet:
            parser.error('The actual .NET 8 SDK is required')
        sdk = re.findall(r'^(8\.0\.\d+) ', run([args.dotnet, '--list-sdks'], 'versions'), re.M)[-1]
        environment.update(DOTNET_CLI_HOME=str(build / 'dotnet-home'), DOTNET_CLI_TELEMETRY_OPTOUT='1',
                           NUGET_PACKAGES=str(build / 'nuget'), DOTNET_CLI_USE_MSBUILD_SERVER='0', MSBUILDDISABLENODEREUSE='1')
        csharp.project(build, sdk)
        run([compiler, '--input', schema, '--language', 'csharp', '--output', build / 'Schema.cs',
             '--csharp.namespace', 'SerializerGenerated'], 'generation')
        (build / 'Program.cs').write_text(CSHARP, encoding='utf-8')
        run([args.dotnet, 'build', build / 'Compression.csproj', '--configuration', 'Release', '--nologo',
             '--disable-build-servers', '-nodeReuse:false'], 'compilation')
        output = run([args.dotnet, build / 'bin/Release/net8.0/Compression.dll', directory], 'execution')
        counts, tool = json.loads(output.splitlines()[-1]), sdk
    else:
        if not args.kotlinc or not args.java:
            parser.error('Actual Kotlin/JDK SDKs are required')
        home = Path(args.kotlinc).resolve().parent.parent
        run([compiler, '--input', schema, '--language', 'kotlin', '--output', build / 'Schema.kt', '--kotlin.package', 'contract'], 'generation')
        (build / 'Main.kt').write_text(KOTLIN, encoding='utf-8')
        command = [args.java, '-Xmx2g', '-cp', home / 'lib/kotlin-compiler.jar',
                   'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-kotlin-home', home, '-jvm-target', '17', '-Werror']
        tool = run(command[:7] + ['-version'], 'version')
        run(command + ['-include-runtime', '-d', build / 'checks.jar', build / 'Schema.kt', build / 'Main.kt'], 'compilation')
        output = run([args.java, '-cp', build / 'checks.jar', 'contract.MainKt', directory], 'execution')
        counts = json.loads(output.splitlines()[-1])
    for label, method in (('default', zipfile.ZIP_DEFLATED), ('runtime', zipfile.ZIP_STORED)):
        with zipfile.ZipFile(directory / ('produced-' + label + '.opc')) as archive:
            assert archive.testzip() is None
            assert archive.getinfo('t.xml').compress_type == method
            assert archive.getinfo('fixed.xml').compress_type == zipfile.ZIP_STORED
    report = {'language': args.language, 'tool': tool, 'decisions': counts, 'independent_inputs': len(rows),
              'independent_outputs': 2, 'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
              'commands': commands, 'elapsed_seconds': round(time.monotonic() - started, 3)}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
