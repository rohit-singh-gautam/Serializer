"""Qualify explicitly owned C presence states with the shared independent XSD corpus."""
import argparse
import importlib.util
from pathlib import Path

from lxml import etree
from portable_xml_presence_test import MODEL, OPTIONS, XSD, BOUND_XSD, RICH_MODEL, RICH_OPTIONS, RICH_XSD, cases, rich_cases

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_c_presence_sdk', ROOT / 'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xml_runner)
sdk = xml_runner.sdk

HEAD = r'''#include "schema.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Require native values and transactional failures, with a useful failing fixture name. */
static void require(bool value, const char* message) { if (!value) { fprintf(stderr,"%s\n",message); exit(EXIT_FAILURE); } }
'''

PROBE = r'''
/* Compare one independent oracle result, native protocol identity, and destination preservation. */
static void probe_@TYPE@(const uint8_t* bytes, size_t size, bool valid, srl_buffer* output, const char* name) {
  @TYPE@ value={0},copy={0};
  require(@TYPE@_init(&value) == srl_ok && @TYPE@_init(&copy) == srl_ok,"Presence initialization");
  const @TYPE@ previous=value;
  const srl_status status=@TYPE@_decode_xml(&value,bytes,size,NULL);
  if (valid) {
    require(status == srl_ok,name);
    require(@TYPE@_encode_xml(&value,output,NULL) == srl_ok,"Presence output");
    require(@TYPE@_decode_xml(&copy,output->data,output->size,NULL) == srl_ok,"Presence roundtrip");
    const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
    for (size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index) {
      srl_buffer first={0},second={0};
      require(@TYPE@_encode(&value,protocols[index],&first) == srl_ok && @TYPE@_encode(&copy,protocols[index],&second) == srl_ok &&
          first.size == second.size && memcmp(first.data,second.data,first.size) == 0,"Presence native protocol identity");
      srl_buffer_free(&first); srl_buffer_free(&second);
    }
    srl_limits bounds=srl_default_limits(); bounds.max_depth=1;
    require(@TYPE@_decode_xml(&copy,bytes,size,&bounds) != srl_ok,"Presence depth bound");
  } else {
    require(status != srl_ok,name);
    require(memcmp(&previous,&value,sizeof(value)) == 0,"Partial C presence candidate published");
  }
  @TYPE@_free(&value); @TYPE@_free(&copy);
}
'''

MAIN = r'''
/* Preserve native absent/nil/empty layers, then execute every independently classified fixture. */
int main(int argc, char** argv) {
  const char* states[]={
    "<sample xmlns='urn:serializer:nil' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'><nil_value xsi:nil='true'/><nested xsi:nil='true'/></sample>",
    "<sample xmlns='urn:serializer:nil' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'><nil_value/><states xsi:nil='true'/><entries xsi:nil='true'/><nested xsi:nil='true'/></sample>",
    "<sample xmlns='urn:serializer:nil' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'><nil_value/><states/><entries/><nested xsi:nil='true'/></sample>"};
  for (size_t index=0;index<sizeof(states)/sizeof(states[0]);++index) {
    nil_fixture_sample value={0}; require(nil_fixture_sample_init(&value) == srl_ok,"Native presence initialization");
    require(nil_fixture_sample_decode_xml(&value,(const uint8_t*)states[index],strlen(states[index]),NULL) == srl_ok,"Native presence decode");
    require(value.states.has_value == (index != 0) && value.entries.has_value == (index != 0) && !value.nested.has_value,"Absent presence layers");
    if (index != 0) { require(value.states.value.has_value == (index == 2) && value.entries.value.has_value == (index == 2),"Nil presence layers"); }
    if (index == 2) { require(value.states.value.value.size == 0 && value.entries.value.value.size == 0,"Empty presence payload"); }
    require(value.nil_value.has_value == (index != 0),"Required nil versus empty"); nil_fixture_sample_free(&value);
  }
  for (int index=1;index<argc;++index) {
    const char* argument=argv[index]; FILE* file=fopen(argument,"rb"); require(file != NULL,"Open presence fixture");
    require(fseek(file,0,SEEK_END) == 0,"Seek presence fixture"); const long length=ftell(file); require(length >= 0,"Presence fixture size"); rewind(file);
    uint8_t* bytes=(uint8_t*)malloc((size_t)length+1); require(bytes != NULL,"Presence fixture allocation");
    require(fread(bytes,1,(size_t)length,file) == (size_t)length,"Read presence fixture"); fclose(file);
    const char* name=strrchr(argument,'/'); if (!name) { name=strrchr(argument,'\\'); } name=name ? name+1 : argument;
    const bool valid=strstr(name,".valid.xml") != NULL; srl_buffer output={0};
    if (strncmp(name,"rich",4) == 0) { probe_nil_shapes_sample(bytes,(size_t)length,valid,&output,name); }
    else if (strncmp(name,"holder",6) == 0) { probe_nil_fixture_holder(bytes,(size_t)length,valid,&output,name); }
    else { probe_nil_fixture_sample(bytes,(size_t)length,valid,&output,name); }
    free(bytes);
    if (valid) {
      const size_t capacity=strlen(argument)+sizeof(".c.xml"); char* path=(char*)malloc(capacity); require(path != NULL,"Output path allocation");
      require(snprintf(path,capacity,"%s.c.xml",argument) >= 0,"Presence output path"); file=fopen(path,"wb"); require(file != NULL,"Open presence output"); free(path);
      require(fwrite(output.data,1,output.size,file) == output.size,"Write presence output"); fclose(file);
    }
    srl_buffer_free(&output);
  }
  puts("C absent, nil, empty, nested and collection states pass"); return EXIT_SUCCESS;
}
'''


def main():
    """Generate actual C, compile through maintained CMake, and independently validate output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    args.build.mkdir(parents=True, exist_ok=True)
    source, config = args.build / 'model.serializer', args.build / 'options.ini'
    source.write_text(MODEL.replace('namespace nil_fixture {', 'namespace nil_fixture { using schema "presence.xsd";') + RICH_MODEL, encoding='utf-8')
    config.write_text(OPTIONS + RICH_OPTIONS, encoding='utf-8')
    (args.build / 'presence.xsd').write_text(BOUND_XSD, encoding='utf-8')
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode()))
    rich_oracle = etree.XMLSchema(etree.fromstring(RICH_XSD.encode()))
    paths = []
    for index, (kind, document, valid) in enumerate(cases() + rich_cases()):
        selected_oracle = rich_oracle if kind == 'rich' else oracle
        if selected_oracle.validate(etree.fromstring(document.encode())) != valid:
            raise AssertionError(f'Independent presence decision differs for {index}: {selected_oracle.error_log}')
        path = args.build / f'{kind}-{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8')
        Path(str(path) + '.c.xml').unlink(missing_ok=True)
        paths.append(path)
    sdk.run([args.compiler, '--input', source, '--config', config, '--language', 'c', '--c.output', args.build / 'schema.h'])
    source_root = args.build / 'source/c'
    source_root.mkdir(parents=True, exist_ok=True)
    consumer = HEAD + ''.join(PROBE.replace('@TYPE@', name) for name in ('nil_fixture_sample', 'nil_fixture_holder', 'nil_shapes_sample')) + MAIN
    (source_root / 'main.c').write_text(consumer, encoding='utf-8')
    runner = sdk.Runner(args)
    launch = xml_runner.build_c(args, runner, args.build, source_root=source_root.parent)
    sdk.run([*launch, *(runner.path('c', item) for item in paths)])
    for path in paths:
        output = Path(str(path) + '.c.xml')
        if path.name.endswith('.valid.xml'):
            (rich_oracle if path.name.startswith('rich') else oracle).assertValid(etree.parse(str(output)))
        elif output.exists():
            raise AssertionError('Invalid presence candidate published output')
    print(f'PASS: {len(paths)} independent C presence decisions', flush=True)


if __name__ == '__main__':
    main()
