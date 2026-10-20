# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Build actual Windows/Linux XML SDKs and retain deterministic, independent and cross-platform replay checks."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import importlib.util
import json
import itertools
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import time
import threading

ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = ('java', 'kotlin', 'csharp', 'go', 'javascript', 'typescript', 'rust', 'swift', 'c')


def digest(path):
    """Hash exact bytes rather than normalized source text or parsed XML."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def linux_path(path):
    """Translate only an actual resolved filesystem path for the explicitly selected WSL host."""
    return subprocess.check_output(['wsl', '--exec', 'wslpath', '-a', '-u', str(Path(path).resolve())],
                                   text=True, encoding='utf-8').strip()


def prepare_environment(args):
    """Select existing real SDKs and isolate mutable caches; no system installation is modified."""
    if args.host == 'windows':
        if args.kotlinc: os.environ['SERIALIZER_KOTLINC'] = str(args.kotlinc.resolve(strict=True))
        if args.cargo_home: os.environ['CARGO_HOME'] = str(args.cargo_home.resolve(strict=True))
        if args.npm_cache: os.environ['npm_config_cache'] = str(args.npm_cache.resolve(strict=True))
        os.environ['CARGO_NET_OFFLINE'] = 'true'
        return
    if os.name != 'nt': raise RuntimeError('The Linux replay adapter requires an explicit Windows-to-WSL host')
    for field in ('linux_cargo', 'linux_dotnet', 'linux_javac', 'kotlinc', 'cargo_home'):
        if getattr(args, field) is None: raise ValueError('Linux replay requires --'+field.replace('_','-'))
    cache = args.build/'sdk-caches'; cache.mkdir(parents=True,exist_ok=True)
    for child in ('registry/cache','registry/index'):
        shutil.copytree(args.cargo_home.resolve(strict=True)/child,cache/'cargo'/child,dirs_exist_ok=True)
    tools = args.build/'sdk-tools'; tools.mkdir(exist_ok=True)
    # Shell wrappers contain only reviewed tool/cache paths; all runtime arguments remain separate argv.
    wrappers = {
        'cargo': 'export CARGO_HOME='+shlex.quote(linux_path(cache/'cargo'))+'\nexport CARGO_NET_OFFLINE=true\nexec '+shlex.quote(linux_path(args.linux_cargo))+' "$@"\n',
        'dotnet': 'export DOTNET_ROOT='+shlex.quote(linux_path(args.linux_dotnet.parent))+'\nexport DOTNET_CLI_HOME='+shlex.quote(linux_path(cache/'dotnet'))+'\nexport DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1\nexec '+shlex.quote(linux_path(args.linux_dotnet))+' "$@"\n',
        'javac': 'exec '+shlex.quote(linux_path(args.linux_javac))+' '+shlex.quote('-J-Djava.home='+linux_path(args.linux_javac.parent.parent))+' "$@"\n',
    }
    for name, source in wrappers.items():
        path=tools/name; path.write_text('#!/bin/sh\n'+source,encoding='utf-8',newline='\n')
        subprocess.run(['wsl','--exec','chmod','700',linux_path(path)],check=True)
        os.environ['SERIALIZER_'+name.upper()]=linux_path(path)
    kotlin = args.kotlinc.resolve(strict=True)
    if kotlin.suffix.lower()=='.bat': kotlin=kotlin.with_suffix('')
    os.environ['SERIALIZER_KOTLINC']=linux_path(kotlin)
    os.environ.update(SERIALIZER_JAVA='/usr/bin/java',
                      SERIALIZER_NODE='/usr/local/bin/node',SERIALIZER_NPM='/usr/local/bin/npm',
                      SERIALIZER_GO='/usr/bin/go',SERIALIZER_SWIFTC='/usr/bin/swiftc')


def main():
    """Replay both fixed/configured profiles with every selected actual SDK and no missing-tool skips."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--host',choices=('windows','linux'),required=True)
    parser.add_argument('--language',default=','.join(LANGUAGES))
    parser.add_argument('--profile',choices=('configured','xsd','both'),default='both')
    parser.add_argument('--libxml2-prefix',type=Path)
    parser.add_argument('--kotlinc',type=Path)
    parser.add_argument('--cargo-home',type=Path)
    parser.add_argument('--npm-cache',type=Path)
    parser.add_argument('--linux-cargo',type=Path)
    parser.add_argument('--linux-dotnet',type=Path)
    parser.add_argument('--linux-javac',type=Path)
    parser.add_argument('--compare',type=Path,help='Previous opposite-host verification.json for bidirectional cross-host exchanges')
    parser.add_argument('--resume',action='store_true',help='Reuse a previously passed exact build, verify its bytes and execute repeat/cross-host consumers again')
    parser.add_argument('--refresh',action='store_true',help='Regenerate with a new compiler; rebuild changed sources and reuse only byte-identical previously executed SDK images')
    parser.add_argument('--jobs',type=int,default=4,help='Maximum independent cross-host consumer processes')
    args=parser.parse_args(); args.build=args.build.resolve(); compiler=args.compiler.resolve(strict=True)
    languages=args.language.split(',')
    if not languages or len(set(languages))!=len(languages) or any(value not in LANGUAGES for value in languages): parser.error('Select distinct names from '+','.join(LANGUAGES))
    if not 1 <= args.jobs <= 16: parser.error('Choose between one and sixteen consumer processes')
    if args.resume and args.refresh: parser.error('Choose either exact resume or compiler refresh')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts: parser.error('In-repository output must remain beneath ignored out/')
    args.build.mkdir(parents=True,exist_ok=True); prepare_environment(args)
    spec=importlib.util.spec_from_file_location('serializer_platform_xml',ROOT/'example/xml/run.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    original_path=module.sdk.Runner.path
    if args.host=='linux':
        mapped_root=linux_path(ROOT).rstrip('/')
        def replay_path(runner,language,path):
            """Reuse the independently queried workspace mount for task paths, avoiding a helper process per argument."""
            absolute=Path(path).resolve()
            if language in runner.wsl and absolute.is_relative_to(ROOT): return mapped_root+'/'+absolute.relative_to(ROOT).as_posix()
            return original_path(runner,language,path)
        module.sdk.Runner.path=replay_path
    manifest=module.fixtures.load_manifest(ROOT/'test/resources/xml_exchange_manifest.json',ROOT)
    previous=json.loads(args.compare.resolve(strict=True).read_text(encoding='utf-8')) if args.compare else None
    if previous and (previous['status']!='passed' or previous['host']==args.host or previous['languages']!=languages): raise ValueError('Comparison requires a passed opposite-host run of the exact same SDK inventory')
    started=time.monotonic(); compiler_hash=digest(compiler)
    report={'status':'running','host':args.host,'orchestrator':platform.platform(),'languages':languages,
            'compiler':str(compiler),'compiler_sha256':compiler_hash,'build':str(args.build),
            'runner_sha256':digest(Path(__file__)),'profiles':{},'commands':[],
            'scope':'Actual configured/fixed-XSD owning XML SDK builds, repeated byte equality, independent expanded-name/value checks and optional bidirectional opposite-host exchanges. This is functional/platform reproducibility, not a throughput or hard peak-memory claim.'}
    report_path=args.build/'verification.json'
    if args.resume or args.refresh:
        existing=json.loads(report_path.read_text(encoding='utf-8'))
        if existing['status']!='passed' or args.resume and existing['compiler_sha256']!=compiler_hash or existing['languages']!=languages or existing['host']!=args.host:
            raise ValueError('Resume requires a passed exact compiler, host and SDK inventory')
        report['commands']=existing['commands']; report['resumed_report_sha256']=digest(report_path)
        report['previous_compiler_sha256']=existing['compiler_sha256']
    command_indexes=itertools.count(len(report['commands'])+1); trace_lock=threading.Lock()

    def run(command,*,cwd=None,capture=False):
        """Retain exact commands, SDK identities and exit status; failed real processes abort the replay."""
        argv=[str(value) for value in command]
        with trace_lock: index=next(command_indexes)
        log=args.build/f'command-{index:04}.log'; beginning=time.monotonic()
        result=subprocess.run(argv,cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=1200)
        log.write_bytes(result.stdout)
        row={'argv':argv,'cwd':str(cwd) if cwd else None,'returncode':result.returncode,
             'elapsed_seconds':round(time.monotonic()-beginning,3),'log':str(log),'log_sha256':digest(log)}
        if len(argv)>=2 and argv[-1] in ('--version','-version','version'): row['sdk_version']=result.stdout.decode('utf-8',errors='replace').strip()
        with trace_lock:
            report['commands'].append(row); report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        if result.returncode: raise RuntimeError('Actual SDK process failed: '+str(argv)+'\n'+result.stdout.decode('utf-8',errors='replace')[-12000:])
        return subprocess.CompletedProcess(argv,0,result.stdout.decode('utf-8') if capture else None)

    original_run=module.sdk.run; original_build=module.build_language
    module.sdk.run=run

    def generated_files(directory, language):
        """Bind strict TypeScript declarations and their actual shared JavaScript runtime together."""
        filename='internal/model/model_gen.go' if language=='go' else module.sdk.OUTPUTS[language]
        files={filename:digest(directory/filename)}
        if language=='typescript': files['schema.mjs']=digest(directory/'schema.mjs')
        return files

    def retained_files(result, language):
        """Recover complete older evidence only when its independently recorded shared runtime exists."""
        if 'generated_files_sha256' in result: return result['generated_files_sha256'][language]
        filename='internal/model/model_gen.go' if language=='go' else module.sdk.OUTPUTS[language]
        files={filename:result['generated_sha256'][language]}
        if language=='typescript':
            if 'javascript' not in result['generated_sha256']: return None
            files['schema.mjs']=result['generated_sha256']['javascript']
        return files

    try:
        run(['wsl','--exec','uname','-a'] if args.host=='linux' else ['cmd','/c','ver'])
        for profile in (('configured','xsd') if args.profile=='both' else (args.profile,)):
            directory=args.build/profile; launches={}; reused=[]; rebuilt=[]
            def build_language(options,runner,language,path,source_root=module.SOURCE):
                """Capture the maintained builder's actual launch command without replacing its SDK work."""
                if args.refresh and generated_files(path,language)==retained_files(existing['profiles'][profile],language):
                    launch=existing['profiles'][profile]['launches'][language];launches[language]=launch;reused.append(language);return launch
                launch=original_build(options,runner,language,path,source_root); launches[language]=list(map(str,launch)); return launch
            module.build_language=build_language
            arguments=['platform-replay','--compiler',str(compiler),'--build',str(directory),'--language',','.join(languages),
                       '--config',str(ROOT/'example/xml'/('codegen.ini' if profile=='configured' else 'xsd.ini'))]
            if args.host=='linux': arguments+=['--wsl-languages',','.join(languages)]
            if args.host=='windows' and args.libxml2_prefix: arguments+=['--libxml2-prefix',str(args.libxml2_prefix.resolve(strict=True))]
            if args.resume:
                launches=existing['profiles'][profile]['launches']
            else:
                sys.argv=arguments; module.main()
            options=argparse.Namespace(wsl_languages=','.join(languages) if args.host=='linux' else '')
            runner=module.sdk.Runner(options)
            generated={}; generated_complete={}; outputs={}
            for language in languages:
                filename='internal/model/model_gen.go' if language=='go' else module.sdk.OUTPUTS[language]
                generated[language]=digest(directory/language/filename); outputs[language]=digest(directory/(language+'.xml'))
                generated_complete[language]=generated_files(directory/language,language)
                if args.resume and (generated_complete[language]!=retained_files(existing['profiles'][profile],language) or outputs[language]!=existing['profiles'][profile]['output_sha256'][language]):
                    raise AssertionError('Retained replay build changed: '+language)
                for repeat in range(2):
                    path=directory/f'{language}-repeat-{repeat}.xml'
                    run([*launches[language],runner.path(language,ROOT/'example/xml/order.xml'),runner.path(language,path)])
                    module.fixtures.check_document(path,manifest['expected'])
                    if digest(path)!=outputs[language]: raise AssertionError('Repeated XML bytes differ: '+language)
            result={'launches':launches,'generated_sha256':generated,'generated_files_sha256':generated_complete,'output_sha256':outputs,
                    'independent_documents':len(languages)*(len(languages)+3),'same_host_consumer_pairs':len(languages)**2,
                    'repeatability_checks':len(languages)*2,'cross_host_consumer_pairs':0,
                    'reused_identical_sdk_images':reused,'rebuilt_sdk_images':[value for value in languages if value not in reused and not args.resume]}
            if previous:
                other=previous['profiles'][profile]
                if other['generated_sha256']!=generated: raise AssertionError('Cross-host regenerated sources differ')
                if any(retained_files(other,language)!=generated_complete[language] for language in languages): raise AssertionError('Cross-host complete generated source sets differ')
                if other['output_sha256']!=outputs: raise AssertionError('Cross-host corresponding XML output bytes differ')
                def exchange(pair):
                    """Execute one independent opposite-host pair in both directions and verify each full document."""
                    producer,consumer=pair
                    foreign=Path(previous['build'])/profile/(producer+'.xml')
                    path=directory/f'foreign-{producer}-to-{consumer}.xml'
                    run([*launches[consumer],runner.path(consumer,foreign),runner.path(consumer,path)])
                    module.fixtures.check_document(path,manifest['expected'])
                    reverse=directory/f'reverse-{producer}-to-{consumer}.xml'
                    source=directory/(producer+'.xml')
                    # The previous run's executable arrays already carry its explicit real host adapter.
                    source_arg=linux_path(source) if previous['host']=='linux' else str(source)
                    output_arg=linux_path(reverse) if previous['host']=='linux' else str(reverse)
                    run([*other['launches'][consumer],source_arg,output_arg]); module.fixtures.check_document(reverse,manifest['expected'])
                    return 2
                with ThreadPoolExecutor(max_workers=args.jobs) as pool:
                    for future in as_completed([pool.submit(exchange,(producer,consumer)) for producer in languages for consumer in languages]):
                        result['cross_host_consumer_pairs']+=future.result()
                result['opposite_host_report_sha256']=digest(args.compare.resolve())
            report['profiles'][profile]=result
            print('PASS: '+args.host+'/'+profile+' '+str(len(languages))+' actual SDKs, repeatability and independent exchanges',flush=True)
        if digest(compiler)!=compiler_hash: raise AssertionError('Compiler changed during replay')
        report['status']='passed'
    except BaseException as failure:
        report['status']='failed';report['failure']=str(failure);raise
    finally:
        module.sdk.run=original_run;module.build_language=original_build
        module.sdk.Runner.path=original_path
        report['elapsed_seconds']=round(time.monotonic()-started,3)
        report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
