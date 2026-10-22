"""Actual XSD 1.0 character-set probes shared by the eleven generated SDK qualifications."""
import atexit
import base64
from pathlib import Path
import subprocess
import tempfile


class Oracle:
    """Borrow one JAXP process: libxml2 misclassifies CJK letters and nested class subtraction."""
    def __init__(self, xsd):
        """Compile one independent schema, keeping provider artifacts beneath ignored out/."""
        root = Path(__file__).resolve().parents[1] / 'out/build'
        root.mkdir(parents=True, exist_ok=True)
        self.directory = tempfile.TemporaryDirectory(prefix='unicode-oracle-', dir=root)
        directory = Path(self.directory.name)
        (directory / 'schema.xsd').write_text(xsd, encoding='utf-8')
        source = directory / 'PatternOracle.java'
        source.write_text('''import java.io.*;import java.nio.charset.StandardCharsets;import java.util.Base64;
import javax.xml.XMLConstants;import javax.xml.transform.stream.StreamSource;import javax.xml.validation.SchemaFactory;
/** Classify untouched documents with an independent XSD 1.0 engine and no external resources. */
public final class PatternOracle {
  /** Reuse only immutable schema metadata; each original document receives a fresh validator. */
  public static void main(String[] args)throws Exception{
    var factory=SchemaFactory.newInstance(XMLConstants.W3C_XML_SCHEMA_NS_URI);
    factory.setProperty(XMLConstants.ACCESS_EXTERNAL_DTD,"");factory.setProperty(XMLConstants.ACCESS_EXTERNAL_SCHEMA,"");
    var schema=factory.newSchema(new File(args[0]));
    var reader=new BufferedReader(new InputStreamReader(System.in,StandardCharsets.US_ASCII));String line;
    while((line=reader.readLine())!=null){try{schema.newValidator().validate(new StreamSource(new ByteArrayInputStream(Base64.getDecoder().decode(line))));System.out.println("true");}
      catch(org.xml.sax.SAXException failure){System.out.println("false");}System.out.flush();}
  }
}''', encoding='utf-8')
        subprocess.run(['javac', '-Xlint:all', '-Werror', str(source)], check=True, capture_output=True)
        self.process = subprocess.Popen(['java', '-cp', str(directory), 'PatternOracle', str(directory / 'schema.xsd')],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.error_log = 'Independent JAXP XSD rejection'
        atexit.register(self.close)

    def validate(self, document):
        """Send exact XML bytes to the independent provider without generated model normalization."""
        from lxml import etree
        data = document if isinstance(document, bytes) else etree.tostring(document)
        self.process.stdin.write(base64.b64encode(data).decode('ascii') + '\n')
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if result not in ('true', 'false'):
            raise RuntimeError('JAXP oracle failed: ' + self.process.stderr.read())
        return result == 'true'

    def assertValid(self, document):
        """Require emitted original documents to remain valid under the independent schema."""
        assert self.validate(document), self.error_log

    def close(self):
        """Close provider pipes and remove only this oracle's scoped temporary directory."""
        if self.process.poll() is None:
            self.process.stdin.close()
            self.process.wait(timeout=30)
        self.process.stdout.close(); self.process.stderr.close()
        self.directory.cleanup()
PATTERNS = (
    (r'\p{L}+', ('abc', 'é', '漢字', '17', '')),
    (r'\p{Lu}\p{Ll}', ('Ab', 'Αβ', 'AA', 'aB')),
    (r'[\p{L}-[AEIOUaeiou]]+', ('bc', 'β', 'a', 'bee', '17')),
    (r'[a-z-[d-f-[e]]]+', ('abceg', 'eee', 'd', 'f', 'azd')),
    (r'[^a-z-[0-9]]+', ('ABC', '!', 'a', '5', '')),
    (r'\p{IsGreek}+', ('α', 'Αβ', 'a', '7')),
    (r'\d+', ('17', '١٢', 'a', '')),
    (r'\s+', (' \t\r\n', '\u00a0', 'x', '')),
    (r'\w+', ('abc17', 'α', '+', '_', ' ', '!')),
    (r'\i\c*', ('Name-17', '_a', ':a', 'α', '17', ' a')),
    (r'\P{L}+', ('17!', ' ', 'a', 'α')),
    (r'[\I\C\D\S\W]+', ('!', ' ', 'a', '17', '')),
)

PREFIXED = '|'.join(chr(ord('E') + index) + '(' + expression + ')' for index, (expression, _) in enumerate(PATTERNS))
SAMPLES = tuple(chr(ord('E') + index) + value for index, (_, values) in enumerate(PATTERNS) for value in values)
