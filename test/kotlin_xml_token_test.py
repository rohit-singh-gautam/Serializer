"""Qualify generated Kotlin token codecs with retained XML node construction disabled."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_kotlin_token_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

MAIN = '''// Exercise the actual generated SDK while forbidding the old retained-tree path.
fun main() {
    System.setProperty("serializer.forbidTree", "true")
    check(runCatching { SrlXmlNode(SrlXmlName("probe")) }.exceptionOrNull()?.message == "retained XML tree")
    val document = "<order xmlns='urn:serializer:xsd' id='4294967295'><customer>A&amp;B&#13;😀</customer><value>-2147483648</value><value>2147483647</value><note></note></order>"
    val bytes = document.toByteArray(Charsets.UTF_8)
    val value = XmlProviderFixtureOrder.decodeXml(bytes)
    check(value.id == UInt.MAX_VALUE && value.customer == "A&B\\r😀" && value.values.contentEquals(intArrayOf(Int.MIN_VALUE, Int.MAX_VALUE)))
    check(value.note.isPresent && value.note.get() == "")
    val encoded = value.encodeXml()
    check(XmlProviderFixtureOrder.decodeXml(encoded).encode(Protocol.BINARY_NONE).contentEquals(value.encode(Protocol.BINARY_NONE)))
    check(XmlProviderFixtureOrder.decodeXml(document.replace("2147483647", "2147<![CDATA[483]]>&#54;47").toByteArray(Charsets.UTF_8)).values[1] == Int.MAX_VALUE)
    check(!XmlProviderFixtureOrder.decodeXml(document.replace("<note></note>", "").toByteArray(Charsets.UTF_8)).note.isPresent)
    check(XmlProviderFixtureOrder.decodeXml(encoded, XmlLimits(maxBytes = encoded.size)).id == UInt.MAX_VALUE)
    for (bad in listOf(document + "<extra/>", document.dropLast(1), document.replace("urn:serializer:xsd", "urn:wrong"),
        document.replace("<order ", "<order unexpected='1' "), document.replace("<value>-2147483648</value>", "<value>2147483648</value>"),
        document.replace("<note>", "<unexpected/><note>"), document.replace("<note>", "<note unexpected='1'>"),
        document.replace("<customer>", "<customer><nested/>"), "<!DOCTYPE order [<!ENTITY local 'expanded'>]>" + document,
        "<?xml version='1.1'?>" + document, "<?xml version='1.0' encoding='UTF-16'?>" + document,
        "<?xml version='1.0' encoding='ISO-8859-1'?>" + document)) {
        check(runCatching { XmlProviderFixtureOrder.decodeXml(bad.toByteArray(Charsets.UTF_8)) }.isFailure) { "Invalid candidate published" }
    }
    for (bad in listOf(byteArrayOf(0xc0.toByte(), 0x80.toByte()), byteArrayOf(0xed.toByte(), 0xa0.toByte(), 0x80.toByte()),
        byteArrayOf(0xf4.toByte(), 0x90.toByte(), 0x80.toByte(), 0x80.toByte()), byteArrayOf(0xe2.toByte(), 0x82.toByte()))) {
        check(runCatching { XmlProviderFixtureOrder.decodeXml(bad) }.isFailure) { "Invalid UTF8 accepted" }
    }
    for (limits in listOf(XmlLimits(maxBytes = bytes.size - 1), XmlLimits(maxNodes = 1), XmlLimits(maxDepth = 1), XmlLimits(maxTextBytes = 1))) {
        check(runCatching { XmlProviderFixtureOrder.decodeXml(bytes, limits) }.isFailure) { "Input bound exceeded" }
    }
    println("Kotlin direct input/output, UTF8, lexical boundaries, limits and transactional failures pass")
}
'''


def main():
    """Generate actual Kotlin, disable tree allocation in its test copy, and use the real SDK."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    output = args.build / 'Schema.kt'
    sdk.run([args.compiler, '--input', ROOT / 'test/resources/xml_provider.serializer',
             '--config', ROOT / 'test/resources/xml_provider.ini', '--language', 'kotlin',
             '--protocols', 'binary_none', '--kotlin.package=', '--kotlin.output', output])
    generated = output.read_text(encoding='utf-8')
    signature = 'internal class SrlXmlNode('
    if generated.count(signature) != 1:
        raise AssertionError('Expected the retained-node constructor for the negative control')
    begin = generated.index('{', generated.index(signature)) + 1
    generated = generated[:begin] + '\n    init { check(System.getProperty("serializer.forbidTree") != "true") { "retained XML tree" } }' + generated[begin:]
    output.write_text(generated, encoding='utf-8')
    (args.build / 'Main.kt').write_text(MAIN, encoding='utf-8')
    runner = sdk.Runner(args)
    path = lambda name: runner.path('kotlin', args.build / name)
    sdk.run(runner.command('kotlin', 'kotlinc', path('Schema.kt'), path('Main.kt'),
                           '-Werror', '-include-runtime', '-d', path('main.jar')))
    sdk.run(runner.command('kotlin', 'java', '-jar', path('main.jar')))


if __name__ == '__main__':
    main()
