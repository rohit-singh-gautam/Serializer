# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual public Kotlin/Swift owning large OPC/DEFLATE/XML operations."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zipfile

import package_large_operation_corpus as corpus

ROOT=Path(__file__).resolve().parents[1]


def digest(path):
    """Identify exact maintained input, compiler, emitted code and output bytes."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    """Build separate generated libraries and public consumers, then inspect complete outputs independently."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--language',choices=('kotlin','swift'),required=True)
    parser.add_argument('--kotlinc',type=Path)
    parser.add_argument('--swiftc',default='swiftc')
    parser.add_argument('--entries-per-registry',type=int,default=corpus.DEFAULT_ENTRIES_PER_REGISTRY)
    parser.add_argument('--oracle-pythonpath',type=Path,help='Existing lxml provider directory; no dependencies are installed')
    args=parser.parse_args()
    compiler,build=args.compiler.resolve(strict=True),args.build.resolve()
    if build==ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository output must remain beneath ignored out/')
    if args.entries_per_registry<1:
        parser.error('Choose a positive finite entries-per-registry count')
    if args.language=='swift' and os.name!='nt':
        parser.error('This selected Swift owning package profile requires its actual Windows SDK')
    if os.name=='nt':
        ctypes.windll.kernel32.SetErrorMode(0x8003)
    if args.oracle_pythonpath:
        sys.path.insert(0,str(args.oracle_pythonpath.resolve(strict=True)))
    import lxml.etree
    build.mkdir(parents=True,exist_ok=True)
    library,consumer,fixtures=(build/name for name in ('library','consumer','fixtures'))
    library.mkdir(exist_ok=True);consumer.mkdir(exist_ok=True)
    started=time.monotonic();commands=[];environment=os.environ.copy()
    report={'status':'running','language':args.language,'compiler_sha256':digest(compiler),
        'runner_sha256':digest(Path(__file__)),'separate_library':True,
        'runtime_xsd_provider_selected':False,'independent_xsd_provider':lxml.etree.LXML_VERSION,
        'policy':{'max_archive_bytes':64<<20,'max_part_bytes':32<<20,'max_expanded_bytes':128<<20,
            'max_buffer_bytes':128<<20,'max_metadata_bytes':4<<20,'max_allocation_bytes':2<<30,
            'max_work_units':4<<30,'max_references':1000000,'max_preserved_bytes':1<<20,
            'max_entries':32,'max_relationships':32}}

    def retain():
        """Keep incremental evidence without claiming an incomplete SDK run passed."""
        (build/'verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

    def run(command,label,timeout=1800):
        """Capture actual SDK diagnostics, exact argv and failure codes with inherited dialog suppression."""
        argv=[str(value) for value in command];before=time.monotonic()
        result=subprocess.run(argv,env=environment,cwd=build,capture_output=True,text=True,
            encoding='utf-8',errors='strict',timeout=timeout)
        elapsed=round(time.monotonic()-before,3)
        (build/(label+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
        commands.append({'argv':argv,'returncode':result.returncode,'elapsed_seconds':elapsed})
        (build/'commands.json').write_text(json.dumps(commands,indent=2)+'\n',encoding='utf-8')
        if result.returncode:
            report.update(status='failed',failed_command=label);retain()
            print(result.stdout+result.stderr,end='');result.check_returncode()
        return result.stdout+result.stderr

    retain()
    inventory=corpus.fixtures(fixtures,args.entries_per_registry)
    report['fixture']=inventory
    report['compiler_version']=run([compiler,'--version'],'compiler-version').strip()
    schema=ROOT/'test/resources/opc_identity.serializer';mapping=ROOT/'test/resources/opc_identity.ini'
    source=library/('Schema.kt' if args.language=='kotlin' else 'Schema.swift')
    run([compiler,'--input',schema,'--config',mapping,'--language',args.language,'--output',source,
        '--protocols','binary_none','--kotlin.package','generated'],'generation')
    report['generated_source']={'bytes':source.stat().st_size,'sha256':digest(source)}
    report['schema_sha256']=digest(schema);report['mapping_sha256']=digest(mapping)
    resources=ROOT/'test/resources'
    if args.language=='kotlin':
        if args.kotlinc is None:
            parser.error('Kotlin requires --kotlinc from an existing actual compiler distribution')
        home=args.kotlinc.resolve(strict=True).parent.parent
        common=['java','-Xmx2g','-cp',home/'lib/kotlin-compiler.jar','org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
            '-kotlin-home',home,'-jvm-target','17','-Werror']
        report['sdk']=run(['java','-version'],'java-version').strip()
        target=consumer/'Main.kt';shutil.copyfile(resources/'package_large_operation_main.kt',target)
        run([*common,'-module-name','generated_large_package','-d',library/'generated.jar',source],'library-compile')
        run([*common,'-module-name','external_large_package','-classpath',library/'generated.jar','-include-runtime',
            '-d',consumer/'consumer.jar',target],'consumer-compile')
        launch=['java','-Xmx2g','-cp',str(consumer/'consumer.jar')+os.pathsep+str(library/'generated.jar'),'MainKt']
    else:
        report['sdk']=run([args.swiftc,'--version'],'swift-version').strip()
        target=consumer/'main.swift'
        target.write_text('import GeneratedLarge\n'+(resources/'package_large_operation_main.swift').read_text(encoding='utf-8'),encoding='utf-8')
        run([args.swiftc,'-O','-warnings-as-errors','-emit-module','-emit-library','-module-name','GeneratedLarge',
            '-emit-module-path',library/'GeneratedLarge.swiftmodule',source,'-o',library/'GeneratedLarge.dll'],'library-compile')
        run([args.swiftc,'-O','-warnings-as-errors','-I',library,'-L',library,'-lGeneratedLarge',target,
            '-o',consumer/'consumer.exe'],'consumer-compile')
        environment['PATH']=str(library)+os.pathsep+environment.get('PATH','')
        launch=[consumer/'consumer.exe']
    report['consumer_sha256']=digest(target);retain()
    output=run([*launch,fixtures,args.entries_per_registry],'acceptance')
    rows=[json.loads(line) for line in output.splitlines() if line.startswith('{')]
    if len(rows)!=1:
        raise AssertionError('Missing actual SDK result')
    counts=rows[0]
    if counts['owners']!=inventory['registries'] or counts['entries_per_owner']!=args.entries_per_registry:
        raise AssertionError('SDK owner/count mismatch')
    if counts['references']!=inventory['identity_tuples']+inventory['relationships']:
        raise AssertionError('Cumulative native identity/package reference ledger mismatch')
    outputs={}
    for name in ('encoded.opc','saved.opc'):
        path=fixtures/name
        corpus.assert_output(path,args.entries_per_registry)
        with zipfile.ZipFile(path) as archive:
            methods={item.filename:item.compress_type for item in archive.infolist()}
            if any(methods[f'Registries/registry{index}.xml']!=zipfile.ZIP_DEFLATED for index in range(3)):
                raise AssertionError('Requested runtime DEFLATE profile was not emitted')
        outputs[name]={'bytes':path.stat().st_size,'sha256':digest(path),'compression_methods':methods}
    if (fixtures/'sentinel.opc').read_bytes()!=bytes((11,22,33)):
        raise AssertionError('Failed atomic Save replaced the existing sentinel')
    if digest(compiler)!=report['compiler_sha256']:
        raise AssertionError('Immutable compiler changed during qualification')
    report.update(status='passed',results=counts,outputs=outputs,elapsed_seconds=round(time.monotonic()-started,3))
    retain();print(json.dumps(report,indent=2),flush=True)


if __name__=='__main__':
    main()
