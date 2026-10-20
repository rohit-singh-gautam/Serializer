# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Exercise cumulative native XML identity/work/allocation budgets inside generated packages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
import xml.etree.ElementTree as etree
import xml.parsers.expat as expat
import zipfile

import python_package_test as independent
from package_node_dependencies import prepare_node_dependencies

ROOT = Path(__file__).resolve().parents[1]


def xml_inventory(payload):
    """Count physical XML inventory with independent Expat, including namespace and expanded-name bytes."""
    counts = [0, 0, 0]
    parser = expat.ParserCreate(namespace_separator='}')

    def expanded_bytes(name):
        """The separator is Expat metadata, so only the original local/URI UTF-8 bytes are admitted."""
        return sum(len(item.encode('utf-8')) for item in name.split('}', 1))

    def namespace(prefix, uri):
        """Namespace declarations are physical attributes even though namespace-aware Expat separates them."""
        counts[1] += 1
        counts[2] += len((prefix or '').encode('utf-8')) + len((uri or '').encode('utf-8'))

    def element(name, attributes):
        """Count each complete expanded name and normalized decoded scalar exactly once."""
        counts[0] += 1
        counts[1] += len(attributes)
        counts[2] += expanded_bytes(name)
        for key, value in attributes.items():
            counts[2] += expanded_bytes(key) + len(value.encode('utf-8'))

    def characters(value):
        """Count decoded text independently of source entity/reference spelling."""
        counts[2] += len(value.encode('utf-8'))

    parser.StartNamespaceDeclHandler = namespace
    parser.StartElementHandler = element
    parser.CharacterDataHandler = characters
    parser.Parse(payload, True)
    return counts


def fixtures(directory):
    """Use three independent XML identity owners with six equal-valued, repeated forward keyrefs."""
    directory.mkdir(parents=True, exist_ok=True)
    parts = independent.independent_identity_parts()
    (directory / 'valid.opc').write_bytes(independent.independent_zip(parts))
    (directory / 'registry.xml').write_bytes(parts['Registries/registry0.xml'])
    inventory = xml_inventory(parts['Registries/registry0.xml'])
    (directory / 'inventory.txt').write_text(' '.join(str(3 * count) for count in inventory) + '\n', encoding='utf-8')
    empty = dict(parts)
    for name in ('Registries/registry0.xml', 'Registries/registry1.xml', 'Registries/registry2.xml'):
        empty[name] = b'<registry xmlns="urn:serializer:identity-test"/>'
    (directory / 'empty.opc').write_bytes(independent.independent_zip(empty))
    wrong_root = dict(empty)
    wrong_root['Registries/registry0.xml'] = b'<other xmlns="urn:serializer:identity-test"/>'
    (directory / 'wrong-root.opc').write_bytes(independent.independent_zip(wrong_root))
    middle = dict(parts)
    middle['Registries/registry1.xml'] = middle['Registries/registry1.xml'].replace(b'<ref id="7"/>', b'<ref id="8"/>')
    (directory / 'middle-invalid.opc').write_bytes(independent.independent_zip(middle))
    middle['Registries/registry1.xml'] = parts['Registries/registry1.xml'].replace(b'<ref id="7"/>', b'<ref id="invalid"/>')
    (directory / 'middle-scalar.opc').write_bytes(independent.independent_zip(middle))
    # Expanded attribute URI inventory can exhaust native text admission while physical bytes still fit.
    header = dict(parts)
    declarations = b'xmlns:h="urn:' + b'h' * 4096 + b'" ' + b' '.join(
        f'h:attribute{index}=""'.encode('ascii') for index in range(9)) + b' '
    header['Registries/registry1.xml'] = parts['Registries/registry1.xml'].replace(
        b'<registry ', b'<registry ' + declarations, 1)
    header_limit = len(header['Registries/registry1.xml'])
    assert all(len(payload) <= header_limit for payload in header.values())
    assert xml_inventory(header['Registries/registry1.xml'])[2] > header_limit
    (directory / 'header-quota.opc').write_bytes(independent.independent_zip(header))
    (directory / 'header-quota-limit.txt').write_text(str(header_limit) + '\n', encoding='utf-8')
    key = 'Registries/registry2.xml'
    parts[key] = parts[key].replace(b'<ref id="7"/>', b'<ref id="8"/>')
    (directory / 'invalid.opc').write_bytes(independent.independent_zip(parts))


JAVA = r'''
import java.nio.file.*;
public final class BudgetMain {
  private static int passed;
  private static void check(boolean condition,String message){if(!condition)throw new AssertionError(message);++passed;}
  public static void main(String[] arguments)throws Exception{
    Path directory=Path.of(arguments[0]);byte[] bytes=Files.readAllBytes(directory.resolve("valid.opc"));var budget=new Identity.SerializerPackage.Budget();var loaded=Identity.IdentityPackage.Archive.load(bytes,budget,null,null);check(loaded.value.registries.size()==3,"complete identity aggregate");
    long references=budget.configured().maxReferences-budget.remaining(Identity.SerializerPackage.Resource.REFERENCES);check(references==15,"three links plus twelve complete key/unique/keyref tuples");
    for(String resource:new String[]{"references","work","allocation"})for(boolean tight:new boolean[]{false,true}){var limits=new Identity.SerializerPackage.Limits();long used=resource.equals("references")?references:resource.equals("work")?budget.configured().maxWorkUnits-budget.remaining(Identity.SerializerPackage.Resource.WORK_UNITS):budget.configured().maxDecoderAllocationBytes-budget.remaining(Identity.SerializerPackage.Resource.DECODER_ALLOCATION_BYTES);if(resource.equals("references"))limits.maxReferences=used-(tight?1:0);else if(resource.equals("work"))limits.maxWorkUnits=used-(tight?1:0);else limits.maxDecoderAllocationBytes=used-(tight?1:0);try{Identity.IdentityPackage.Archive.load(bytes,new Identity.SerializerPackage.Budget(limits),null,null);check(!tight,"one-tight native "+resource+" accepted");}catch(Identity.SerializerPackage.PackageError failure){check(tight&&failure.code==Identity.SerializerPackage.Code.RESOURCE_LIMIT,"shared "+resource);check(!failure.part.isEmpty()&&!failure.fieldPath.isEmpty()&&!failure.relationshipId.isEmpty(),"precise native "+resource+" context");}}
    boolean[] published={false};try{Identity.IdentityPackage.Archive.load(Files.readAllBytes(directory.resolve("invalid.opc")),new Identity.SerializerPackage.Budget(),null,(candidate,shared)->published[0]=true);throw new AssertionError("invalid keyref accepted");}catch(Identity.SerializerPackage.PackageError failure){check(failure.code==Identity.SerializerPackage.Code.CODEC&&!published[0],"native validation atomicity");}
    Files.write(directory.resolve("produced.opc"),Identity.IdentityPackage.Archive.encode(loaded.value));System.out.println("{\"passed\":"+passed+",\"native_references\":"+references+"}");
  }
}
'''

GO = r'''
package main
import("os";"testing";"errors";"fmt")
func TestSharedCodecBudget(t *testing.T){passed:=0;check:=func(value bool,message string){t.Helper();if !value{t.Fatal(message)};passed++};bytes,err:=os.ReadFile("fixtures/valid.opc");if err!=nil{t.Fatal(err)};budget,err:=SerializerPackageNewBudget();if err!=nil{t.Fatal(err)};loaded,err:=IdentityPackageArchiveLoad(bytes,IdentityPackageArchiveLoadOptions{Budget:budget});if err!=nil{t.Fatal(err)};check(len(loaded.Registries)==3,"complete native aggregate");references:=budget.used[SerializerPackageReferences];check(references==15,"three links plus twelve complete key/unique/keyref tuples")
for _,resource:=range []string{"references","work","allocation"}{for _,tight:=range []bool{false,true}{limits:=SerializerPackageDefaultLimits();used:=references;if resource=="work"{used=budget.used[SerializerPackageWorkUnits]}else if resource=="allocation"{used=budget.used[SerializerPackageDecoderAllocationBytes]};if tight{used--};switch resource{case "references":limits.MaxReferences=used;case "work":limits.MaxWorkUnits=used;case "allocation":limits.MaxDecoderAllocationBytes=used};policy,e:=SerializerPackageNewBudget(limits);if e!=nil{t.Fatal(e)};value,e:=IdentityPackageArchiveLoad(bytes,IdentityPackageArchiveLoadOptions{Budget:policy});if !tight{check(e==nil&&value!=nil,"exact "+resource+fmt.Sprint(e))}else{var failure *SerializerPackageError;check(value==nil&&errors.As(e,&failure)&&failure.Code==SerializerPackageResourceLimit,"tight native "+resource+fmt.Sprint(e));check(failure.Part!=""&&failure.FieldPath!=""&&failure.RelationshipID!="","precise native "+resource+fmt.Sprintf(" %+v",failure))}}}
invalid,err:=os.ReadFile("fixtures/invalid.opc");if err!=nil{t.Fatal(err)};published:=false;value,err:=IdentityPackageArchiveLoad(invalid,IdentityPackageArchiveLoadOptions{Validate:func(*IdentityPackageArchiveValue,*SerializerPackageBudget)error{published=true;return nil}});var failure *SerializerPackageError;check(value==nil&&errors.As(err,&failure)&&failure.Code==SerializerPackageCodec&&!published,"native validation atomicity");output,err:=IdentityPackageArchiveEncode(loaded);if err!=nil{t.Fatal(err)};if err=os.WriteFile("fixtures/produced.opc",output,0600);err!=nil{t.Fatal(err)};fmt.Printf("{\"passed\":%d,\"native_references\":%d}\n",passed,references)
}
'''

JAVASCRIPT = r'''
import{readFile,writeFile}from'node:fs/promises';import{IdentityPackageArchive as A,SerializerPackage as P,SerializerCompression as C}from'./identity.mjs';
const directory=new URL('./fixtures/',import.meta.url),bytes=new Uint8Array(await readFile(new URL('valid.opc',directory))),provider=await C.nodeProvider(),options={compressionProvider:provider},budget=new P.Budget();let passed=0;function check(value,message){if(!value)throw Error(message);++passed;}
const loaded=await A.load(bytes,{...options,budget});check(loaded.value.registries.length===3,'complete native aggregate');const references=budget.used.references;check(references===15,'three links plus twelve complete key/unique/keyref tuples');
for(const resource of['references','workUnits','decoderAllocationBytes'])for(const tight of[false,true]){const limit={references:'maxReferences',workUnits:'maxWorkUnits',decoderAllocationBytes:'maxDecoderAllocationBytes'}[resource];try{await A.load(bytes,{...options,budget:new P.Budget(new P.Limits({[limit]:budget.used[resource]-(tight?1:0)}))});check(!tight,'one-tight accepted '+resource);}catch(error){check(tight&&error instanceof P.Error&&error.code==='resource_limit','native '+resource+' '+error);check(!!error.part&&!!error.fieldPath&&!!error.relationshipId,'precise native '+resource+' '+JSON.stringify(error));}}
let published=false;try{await A.load(new Uint8Array(await readFile(new URL('invalid.opc',directory))),{...options,validate(){published=true;}});throw Error('invalid keyref accepted');}catch(error){check(error instanceof P.Error&&error.code==='invalid_metadata'&&!published,'native atomicity');}await writeFile(new URL('produced.opc',directory),await A.encode(loaded.value,options));console.log(JSON.stringify({passed,native_references:references}));
'''


def main():
    """Compile real generated backends and retain exact scalar/event/identity acceptance evidence."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--language', required=True, choices=('java', 'go', 'javascript', 'c', 'rust'))
    parser.add_argument('--java', default='java')
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--go', default='go')
    parser.add_argument('--node', default='node')
    parser.add_argument('--node-dependencies', type=Path)
    parser.add_argument('--cc', default=shutil.which('cl') if os.name == 'nt' else shutil.which('cc'))
    parser.add_argument('--provider-prefix', type=Path)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--cargo', default='cargo')
    args = parser.parse_args()
    build, compiler = args.build.resolve(), args.compiler.resolve(strict=True)
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain below ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    (build / 'verification.json').unlink(missing_ok=True)
    fixtures(build / 'fixtures')
    started, commands = time.monotonic(), []
    environment = os.environ.copy()
    environment.update(GOWORK='off', GOCACHE=str(build / 'gocache'), GOFLAGS='-mod=readonly')
    prefix = args.provider_prefix.resolve() if args.provider_prefix else None
    if prefix and os.name == 'nt':
        environment['PATH'] = str(prefix / 'bin') + os.pathsep + environment.get('PATH', '')
    if os.name == 'nt':
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)

    def run(command, name, allow_failure=False):
        """Retain independent SDK failures and full diagnostics beside the generated source."""
        argv = [str(item) for item in command]
        result = subprocess.run(argv, cwd=build, env=environment, capture_output=True, text=True,
                                encoding='utf-8', timeout=600)
        (build / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append({'argv': argv, 'returncode': result.returncode})
        (build / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        if result.returncode and not allow_failure:
            print(result.stdout + result.stderr, end='')
            result.check_returncode()
        return result.stdout + result.stderr

    base = [compiler, '--input', ROOT / 'test/resources/opc_identity.serializer',
            '--config', ROOT / 'test/resources/opc_identity.ini']
    original_base = base.copy()
    original_counts = None
    original_source = None
    toolchain = None
    if args.language in ('c', 'rust'):
        base[2] = ROOT / 'test/resources/native_xml_operation.serializer'
        base[4] = ROOT / 'test/resources/native_xml_operation.ini'
    if args.language == 'java':
        run(base + ['--language', 'java', '--output', build / 'Identity.java'], 'generation')
        (build / 'BudgetMain.java').write_text(JAVA, encoding='utf-8')
        run([args.javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
             '-d', build / 'classes', build / 'Identity.java', build / 'BudgetMain.java'], 'compile')
        output = run([args.java, '-cp', build / 'classes', 'BudgetMain', build / 'fixtures'], 'sdk')
        source = build / 'Identity.java'
    elif args.language == 'go':
        run(base + ['--language', 'go', '--go.output', build / 'identity.go', '--go.package', 'main'], 'generation')
        (build / 'go.mod').write_text('module serializer-shared-budget\n\ngo 1.23\n', encoding='utf-8')
        (build / 'budget_test.go').write_text(GO, encoding='utf-8')
        output = run([args.go, 'test', '-count=1', '-v', '.'], 'sdk')
        source = build / 'identity.go'
    elif args.language == 'javascript':
        if not args.node_dependencies:
            parser.error('Provide actual repository-locked Node dependencies')
        prepare_node_dependencies(build, args.node_dependencies, run)
        run(base + ['--language', 'js', '--js.output', build / 'identity.mjs'], 'generation')
        (build / 'main.mjs').write_text(JAVASCRIPT, encoding='utf-8')
        output = run([args.node, build / 'main.mjs'], 'sdk')
        source = build / 'identity.mjs'
    elif args.language == 'c':
        if not args.cc:
            parser.error('Provide an actual C11 compiler in --cc')
        source = build / 'identity.h'
        def generate_c(arguments, name, nested):
            """Generate and render each real schema contract without copying runtime overlays."""
            run(arguments + ['--language', 'c', '--output', source], name)
            generated = source.read_text(encoding='utf-8')
            helper = re.search(r'/\* Generated C package prefix: (\w+) \*/', generated).group(1)
            selected = re.search(r'/\* Generated C selected-codec prefix: (\w+) \*/', generated).group(1)
            text = (ROOT / 'test/resources/package_shared_codec_budget_main.c').read_text(encoding='utf-8')
            (build / 'main.c').write_text('#define SERIALIZER_NATIVE_OPERATION_NESTED_TESTS ' + str(int(nested)) + '\n' +
                text.replace('@p@', helper).replace('@selected@', selected), encoding='utf-8')
        generate_c(original_base, 'original-generation', False)
        original_source = build / 'original_identity.h'
        shutil.copyfile(source, original_source)
        executable = build / ('consumer.exe' if os.name == 'nt' else 'consumer')
        msvc = Path(args.cc).name.lower() in ('cl', 'cl.exe')
        toolchain = run([args.cc, '/Bv'] if msvc else [args.cc, '--version'], 'toolchain', allow_failure=msvc).strip()
        if msvc:
            command = [args.cc, '/nologo', '/std:c11', '/W4', '/WX', '/O2', '/utf-8',
                       '/D_CRT_SECURE_NO_WARNINGS', '/DSERIALIZER_COMPRESSION_ZLIB=1', build / 'main.c',
                       '/Fe' + str(executable), '/Fo' + str(build / 'main.obj')]
            if prefix:
                command += ['/I' + str(prefix / 'include'), '/I' + str(prefix / 'include/libxml2')]
            command += ['/link'] + (['/LIBPATH:' + str(prefix / 'lib')] if prefix else []) + ['libxml2.lib', 'z.lib']
        else:
            command = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                       '-DSERIALIZER_COMPRESSION_ZLIB=1', build / 'main.c', '-o', executable]
            command += subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libxml-2.0'], text=True).split()
            command += ['-lz', '-lm', '-pthread']
        run(command, 'original-compile')
        original_output = run([executable, build / 'fixtures'], 'original-sdk')
        original_counts = [json.loads(line) for line in original_output.splitlines() if line.startswith('{')][-1]
        generate_c(base, 'generation', True)
        run(command, 'compile')
        output = run([executable, build / 'fixtures'], 'sdk')
        if args.sanitize:
            sanitized = build / ('consumer_asan.exe' if os.name == 'nt' else 'consumer_asan')
            checked = [str(value).replace(str(executable), str(sanitized)).replace(str(build / 'main.obj'),
                       str(build / 'main_asan.obj')) for value in command if value != '/O2']
            checked[1:1] = (['/Od', '/Zi', '/fsanitize=address', '/Fd' + str(build / 'native_asan.pdb')] if msvc
                            else ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'])
            run(checked, 'sanitize-compile')
            run([sanitized, build / 'fixtures'], 'sanitize-sdk')
    else:
        toolchain = run([args.cargo, '--version'], 'toolchain').strip()
        source = build / 'identity.rs'
        def generate_rust(arguments, name):
            """Exercise ordinary and collision-bearing schemas through actual emitted trait implementations."""
            run(arguments + ['--language', 'rust', '--rust.output', source], name)
            generated = source.read_text(encoding='utf-8')
            helper = re.search(r'// Generated Rust package helper module: (\w+)', generated).group(1)
            operation = re.search(r'// Generated Rust XML operation module: (\w+)', generated).group(1)
            (build / 'main.rs').write_text((ROOT / 'test/resources/package_shared_codec_budget_main.rs').read_text(
                encoding='utf-8').replace('@p@', helper).replace('@operation@', operation), encoding='utf-8')
            return operation
        generate_rust(original_base, 'original-generation')
        original_source = build / 'original_identity.rs'
        shutil.copyfile(source, original_source)
        for name in ('Cargo.toml', 'Cargo.lock'):
            shutil.copyfile(ROOT / 'example/xml/rust' / name, build / name)
        with (build / 'Cargo.toml').open('a', encoding='utf-8') as manifest:
            manifest.write('\n[features]\nnested = []\n')
        original_output = run([args.cargo, 'run', '--locked', '--offline', '--release', '--manifest-path',
                      build / 'Cargo.toml', '--', build / 'fixtures'], 'original-sdk')
        original_counts = [json.loads(line) for line in original_output.splitlines() if line.startswith('{')][-1]
        assert generate_rust(base, 'generation') != 'serializer_xml_operation', 'Native helper must move around existing schema names'
        output = run([args.cargo, 'run', '--locked', '--offline', '--release', '--manifest-path',
                      build / 'Cargo.toml', '--features', 'nested', '--', build / 'fixtures'], 'sdk')
    counts = [json.loads(line) for line in output.splitlines() if line.startswith('{')][-1]
    with zipfile.ZipFile(build / 'fixtures/produced.opc') as archive:
        assert archive.testzip() is None
        for name in (name for name in archive.namelist() if name.startswith('Registries/')):
            root = etree.fromstring(archive.read(name))
            assert [int(child.attrib['id']) for child in root] == [7, 7, 7]
    report = {'language': args.language, 'sdk': counts, 'independent_inputs': 4 if args.language in ('c', 'rust') else 2, 'independent_outputs': 1,
              'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
              'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'elapsed_seconds': round(time.monotonic() - started, 3)}
    if original_counts is not None:
        report['original_sdk'] = original_counts
        report['original_source_sha256'] = hashlib.sha256(original_source.read_bytes()).hexdigest()
        report['toolchain'] = toolchain
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
