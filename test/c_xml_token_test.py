"""Compile generated C pull codecs with tracked DOM calls and transactional failure probes."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_c_token_sdk', ROOT / 'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xml_runner)
sdk = xml_runner.sdk

DOCUMENT = "<order xmlns='urn:serializer:xsd' id='4294967295'><customer>A&amp;B&#13;😀</customer><value>-2147483648</value><value>2147<![CDATA[483]]>&#54;47</value><note/></order>"
MAIN = r'''#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>
#include <libxml/tree.h>
#include <libxml/xmlschemas.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static unsigned tree_calls;
/* Track explicit generated DOM entry points after the provider headers have been included. */
static xmlDocPtr tracked_xml_read_memory(const char* bytes, int size, const char* url, const char* encoding, int options) {
  ++tree_calls;
  return xmlReadMemory(bytes, size, url, encoding, options);
}
#define xmlReadMemory tracked_xml_read_memory
#include "schema.h"
#undef xmlReadMemory
/* Stop with a useful failure message while letting successful generated ownership cleanup run. */
static void require(bool value, const char* message) { if (!value) { fprintf(stderr, "%s\n", message); exit(EXIT_FAILURE); } }
/* Qualify complete typed input and require invalid calls to preserve every existing native byte. */
int main(int argc, char** argv) {
  const char* document = "@DOCUMENT@";
  xmlDocPtr control = tracked_xml_read_memory(document, (int)strlen(document), NULL, "UTF-8", XML_PARSE_NONET);
  require(control && tree_calls == 1, "DOM negative control failed"); xmlFreeDoc(control); tree_calls = 0;
  xml_provider_fixture_order value = {0}, copy = {0};
  require(xml_provider_fixture_order_init(&value) == srl_ok && xml_provider_fixture_order_init(&copy) == srl_ok, "Owning model init");
  require(xml_provider_fixture_order_decode_xml(&value, (const uint8_t*)document, strlen(document), NULL, NULL) == srl_ok, "Direct C input");
  require(value.id == UINT32_MAX && strcmp(value.customer.data, "A&B\r😀") == 0 && value.values.size == 2 &&
      value.values.data[0] == INT32_MIN && value.values.data[1] == INT32_MAX && value.note.has_value && value.note.value.size == 0, "Decoded lexical values");
  srl_buffer encoded = {0}; require(xml_provider_fixture_order_encode_xml(&value, &encoded, NULL, NULL) == srl_ok, "Direct C output");
  require(xml_provider_fixture_order_decode_xml(&copy, encoded.data, encoded.size, NULL, NULL) == srl_ok, "C output roundtrip");
  const srl_protocol protocols[] = {srl_json, srl_binary_none, srl_binary_integer, srl_binary_string};
  for (size_t index = 0; index < sizeof(protocols)/sizeof(protocols[0]); ++index) {
    srl_buffer first = {0}, second = {0};
    require(xml_provider_fixture_order_encode(&value, protocols[index], &first) == srl_ok &&
        xml_provider_fixture_order_encode(&copy, protocols[index], &second) == srl_ok && first.size == second.size && memcmp(first.data, second.data, first.size) == 0, "Native protocol equality");
    srl_buffer_free(&first); srl_buffer_free(&second);
  }
  srl_limits exact = srl_default_limits(); exact.max_bytes = strlen(document);
  require(xml_provider_fixture_order_decode_xml(&copy, (const uint8_t*)document, strlen(document), &exact, NULL) == srl_ok, "Exact byte boundary");
  srl_buffer before = {0}; require(xml_provider_fixture_order_encode(&copy, srl_binary_none, &before) == srl_ok, "Transactional control");
  for (int index = 1; index < argc; ++index) {
    FILE* file = fopen(argv[index], "rb"); require(file != NULL, "Open invalid fixture");
    require(fseek(file, 0, SEEK_END) == 0, "Size invalid fixture"); const long length = ftell(file); require(length >= 0, "Invalid fixture size"); rewind(file);
    uint8_t* bytes = (uint8_t*)malloc((size_t)length + 1); require(bytes != NULL, "Fixture allocation");
    require(fread(bytes, 1, (size_t)length, file) == (size_t)length, "Read invalid fixture"); fclose(file);
    require(xml_provider_fixture_order_decode_xml(&copy, bytes, (size_t)length, NULL, NULL) != srl_ok, argv[index]); free(bytes);
    srl_buffer after = {0}; require(xml_provider_fixture_order_encode(&copy, srl_binary_none, &after) == srl_ok && before.size == after.size && memcmp(before.data, after.data, before.size) == 0, "Partial C candidate published"); srl_buffer_free(&after);
  }
  for (unsigned selected = 0; selected < 4; ++selected) {
    srl_limits limits = srl_default_limits();
    if (selected == 0) { limits.max_bytes = strlen(document) - 1; }
    else if (selected == 1) { limits.max_elements = 1; }
    else if (selected == 2) { limits.max_depth = 1; }
    else { limits.max_string_bytes = 1; }
    require(xml_provider_fixture_order_decode_xml(&copy, (const uint8_t*)document, strlen(document), &limits, NULL) != srl_ok, "C XML input bound ignored");
  }
  require(tree_calls == 0, "Generated C input retained a DOM");
  xml_provider_fixture_order_free(&value); xml_provider_fixture_order_free(&copy); srl_buffer_free(&before); srl_buffer_free(&encoded);
  puts("C direct owning input, no DOM calls, native protocols, limits and transactional failures pass");
  return EXIT_SUCCESS;
}
'''.replace('@DOCUMENT@', DOCUMENT)


def main():
    """Generate native C and use the maintained CMake/provider workflow for strict compilation."""
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
    sdk.run([args.compiler, '--input', ROOT / 'test/resources/xml_provider.serializer',
             '--config', ROOT / 'test/resources/xml_provider.ini', '--language', 'c', '--c.output', args.build / 'schema.h'])
    source = args.build / 'source' / 'c'
    source.mkdir(parents=True, exist_ok=True)
    (source / 'main.c').write_text(MAIN, encoding='utf-8')
    invalid = [DOCUMENT + '<extra/>', DOCUMENT[:-1], DOCUMENT.replace('urn:serializer:xsd', 'urn:wrong'),
               DOCUMENT.replace('<order ', '<order extra="1" '), DOCUMENT.replace('<note/>', '<note extra="1"/>'),
               DOCUMENT.replace('<customer>', '<customer><nested/>'), DOCUMENT.replace('<note/>', '<note/><note/>'),
               DOCUMENT.replace('</order>', '</wrong>'), DOCUMENT.replace('-2147483648', '2147483648'),
               '<!DOCTYPE order [<!ENTITY local "expanded">]>' + DOCUMENT,
               '<?xml version="1.1"?>' + DOCUMENT, '<?xml version="1.0" encoding="ISO-8859-1"?>' + DOCUMENT,
               DOCUMENT.replace('A&amp;B', '&#0;'), DOCUMENT.replace('A&amp;B', '&#xD800;')]
    paths = []
    for index, document in enumerate(invalid):
        path = args.build / f'invalid-{index}.xml'
        path.write_text(document, encoding='utf-8')
        paths.append(path)
    malformed = args.build / 'invalid-utf8.xml'
    malformed.write_bytes(b'\xc0\x80')
    paths.append(malformed)
    runner = sdk.Runner(args)
    launch = xml_runner.build_c(args, runner, args.build, source_root=source.parent)
    sdk.run([*launch, *(runner.path('c', path) for path in paths)])


if __name__ == '__main__':
    main()
