"""Qualify managed XML lexical lists, independent unions, defaults and exact scalar values."""
import argparse
from decimal import Decimal
import importlib.util
from pathlib import Path
import re
import subprocess

from lxml import etree


ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_scalar_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

MODEL = '''serializer version 1.20.0; namespace scalars { using schema "scalars.xsd";
class sample {
  public string decimal_value; public string integer_value; public string sign;
  public string token; public string language; public string name; public string ncname;
  public string nmtoken; public string union_value; public string list_value;
  public string bool_list; public string fixed_text; public string default_value;
  public array[3] uint16 typed xml(attribute, list);
  public optional<array<bool>> typed_optional xml(attribute, list);
  public optional<string> unit;
  public string default_attribute; public bool fixed_flag;
}
class amount { public uint64 value; public optional<string> unit; }
class words { public array[2] string value xml(text, list); }
class numbers { public array[2] uint16 value xml(text, list); }
class items { public array string value xml(text, list); }
class fixed_list { public array[2] string value xml(text, list); }
class child_list { public array[2] uint16 values xml(list); }
class optional_child { public optional<array<uint16>> values xml(list); }
class list_all { public uint32 id; public array[2] uint16 values xml(list); }
class plain_default { public string value; }
class guard { public string value; }
}'''

XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
<xs:simpleType name="ExactDecimal"><xs:restriction base="xs:decimal"><xs:minExclusive value="-1.0000000000000000000000000001"/><xs:maxInclusive value="999999999999999999999999999.12345678901234567890123456789"/></xs:restriction></xs:simpleType>
<xs:simpleType name="HugeInteger"><xs:restriction base="xs:integer"><xs:minInclusive value="-99999999999999999999999999999999999999"/><xs:maxExclusive value="99999999999999999999999999999999999999"/></xs:restriction></xs:simpleType>
<xs:simpleType name="SmallInteger"><xs:restriction base="xs:unsignedShort"><xs:minInclusive value="10"/><xs:maxInclusive value="20"/><xs:pattern value="[+]?[0-9]+"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Status"><xs:restriction base="xs:token"><xs:enumeration value="ready"/><xs:enumeration value="pending"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Bytes"><xs:restriction><xs:simpleType><xs:list itemType="xs:unsignedByte"/></xs:simpleType><xs:length value="3"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Choice"><xs:union memberTypes="SmallInteger Status Bytes"/></xs:simpleType>
<xs:simpleType name="Pairs"><xs:restriction><xs:simpleType><xs:list itemType="xs:unsignedShort"/></xs:simpleType><xs:length value="2"/><xs:enumeration value="1 2"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Bools"><xs:restriction><xs:simpleType><xs:list itemType="xs:boolean"/></xs:simpleType><xs:length value="2"/><xs:enumeration value="true false"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Three"><xs:restriction><xs:simpleType><xs:list itemType="xs:unsignedShort"/></xs:simpleType><xs:length value="3"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Flags"><xs:restriction><xs:simpleType><xs:list itemType="xs:boolean"/></xs:simpleType><xs:maxLength value="2"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Unit"><xs:restriction base="xs:token"><xs:enumeration value="m"/><xs:enumeration value="cm"/></xs:restriction></xs:simpleType>
<xs:element name="sample"><xs:complexType><xs:sequence>
<xs:element name="decimal_value" type="ExactDecimal"/>
<xs:element name="integer_value" type="HugeInteger"/>
<xs:element name="sign" type="xs:positiveInteger"/>
<xs:element name="token"><xs:simpleType><xs:restriction base="xs:token"><xs:minLength value="1"/><xs:maxLength value="10"/></xs:restriction></xs:simpleType></xs:element>
<xs:element name="language" type="xs:language"/><xs:element name="name" type="xs:Name"/>
<xs:element name="ncname" type="xs:NCName"/><xs:element name="nmtoken" type="xs:NMTOKEN"/>
<xs:element name="union_value" type="Choice"/><xs:element name="list_value" type="Pairs"/>
<xs:element name="bool_list" type="Bools"/><xs:element name="fixed_text" type="xs:token" fixed="done"/>
<xs:element name="default_value" type="xs:token" default="fallback"/>
</xs:sequence><xs:attribute name="typed" type="Three" use="required"/>
<xs:attribute name="typed_optional" type="Flags"/><xs:attribute name="unit" type="Unit" default="m"/>
<xs:attribute name="default_attribute" type="Unit" default="cm"/><xs:attribute name="fixed_flag" type="xs:boolean" fixed="true"/>
</xs:complexType></xs:element>
<xs:simpleType name="Amount"><xs:restriction base="xs:unsignedLong"><xs:minInclusive value="9007199254740992"/><xs:maxExclusive value="9007199254740994"/></xs:restriction></xs:simpleType>
<xs:element name="amount"><xs:complexType><xs:simpleContent><xs:extension base="Amount"><xs:attribute name="unit" type="Unit" default="m"/></xs:extension></xs:simpleContent></xs:complexType></xs:element>
<xs:simpleType name="Words"><xs:restriction><xs:simpleType><xs:list itemType="xs:NMTOKEN"/></xs:simpleType><xs:length value="2"/></xs:restriction></xs:simpleType>
<xs:element name="words"><xs:complexType><xs:simpleContent><xs:extension base="Words"/></xs:simpleContent></xs:complexType></xs:element>
<xs:simpleType name="Numbers"><xs:restriction><xs:simpleType><xs:list itemType="xs:unsignedShort"/></xs:simpleType><xs:length value="2"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Items"><xs:list itemType="xs:token"/></xs:simpleType>
<xs:element name="numbers"><xs:complexType><xs:simpleContent><xs:extension base="Numbers"/></xs:simpleContent></xs:complexType></xs:element>
<xs:element name="items"><xs:complexType><xs:simpleContent><xs:extension base="Items"/></xs:simpleContent></xs:complexType></xs:element>
<xs:element name="fixed_list"><xs:complexType><xs:simpleContent><xs:extension base="Items"/></xs:simpleContent></xs:complexType></xs:element>
<xs:element name="child_list"><xs:complexType><xs:sequence><xs:element name="values" type="Numbers"/></xs:sequence></xs:complexType></xs:element>
<xs:simpleType name="OptionalNumbers"><xs:restriction><xs:simpleType><xs:list itemType="xs:unsignedShort"/></xs:simpleType><xs:maxLength value="2"/></xs:restriction></xs:simpleType>
<xs:element name="optional_child"><xs:complexType><xs:sequence><xs:element name="values" type="OptionalNumbers" minOccurs="0"/></xs:sequence></xs:complexType></xs:element>
<xs:element name="list_all"><xs:complexType><xs:all><xs:element name="id" type="xs:unsignedInt"/><xs:element name="values" type="Numbers"/></xs:all></xs:complexType></xs:element>
<xs:element name="plain_default" default="fallback"><xs:complexType><xs:simpleContent><xs:extension base="xs:string"/></xs:simpleContent></xs:complexType></xs:element>
<xs:element name="guard"><xs:complexType><xs:sequence><xs:element name="value"><xs:simpleType><xs:union>
<xs:simpleType><xs:restriction base="xs:string"><xs:pattern value="a{4096}a{1000}"/></xs:restriction></xs:simpleType>
<xs:simpleType><xs:restriction base="xs:string"/></xs:simpleType>
</xs:union></xs:simpleType></xs:element></xs:sequence></xs:complexType></xs:element>
</xs:schema>'''


def oracle_schema():
    """Keep independent XSD shape checks within libxml2's finite decimal precision."""
    # The generated contract retains the original arbitrary-precision bounds. Python's
    # separate Decimal/int oracle below checks those fields before safe representatives
    # reach libxml2, whose decimal implementation rejects values beyond 24 digits.
    portable = XSD.replace('-1.0000000000000000000000000001', '-1')
    portable = portable.replace('999999999999999999999999999.12345678901234567890123456789', '100000000000000000000')
    portable = portable.replace('-99999999999999999999999999999999999999', '-100000000000000000000')
    portable = portable.replace('99999999999999999999999999999999999999', '100000000000000000000')
    return etree.XMLSchema(etree.fromstring(portable.encode()))


def independent(document, schema):
    """Check arbitrary precision with independent Python arithmetic, and all other rules with XSD."""
    root = etree.fromstring(document)
    if root.tag == 'fixed_list' and len(re.split(r'[ \t\r\n]+', (root.text or '').strip(' \t\r\n'))) != 2:
        return False
    if root.tag == 'sample':
        for field, representative in [('decimal_value', '.5'), ('integer_value', '1'), ('sign', '1')]:
            item = root.find(field)
            if item is None:
                continue
            text = (item.text or '').strip(' \t\r\n')
            if field == 'decimal_value':
                if not re.fullmatch(r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)', text):
                    return False
                if not Decimal('-1.0000000000000000000000000001') < Decimal(text) <= Decimal('999999999999999999999999999.12345678901234567890123456789'):
                    return False
            else:
                if not re.fullmatch(r'[+-]?[0-9]+', text):
                    return False
                number = int(text)
                if field == 'integer_value' and not -99999999999999999999999999999999999999 <= number < 99999999999999999999999999999999999999:
                    return False
                if field == 'sign' and number <= 0:
                    return False
            item.text = representative
        # These presentations denote identical XSD values. Normalize the oracle's
        # view because this libxml2 build rejects '+' on unsigned values and checks
        # token fixed constraints before applying its whitespace normalization.
        for field in ('union_value', 'list_value'):
            item = root.find(field)
            if item is not None and item.text:
                item.text = re.sub(r'(?<!\S)\+([0-9]+)(?!\S)', r'\1', item.text)
        fixed = root.find('fixed_text')
        if fixed is not None and fixed.text is not None:
            fixed.text = ' '.join(re.split(r'[ \t\r\n]+', fixed.text.strip(' \t\r\n')))
    return schema.validate(root)


def cases():
    """Specify value-space endpoints, lexical aliases and isolated invalid branches independently."""
    fields = dict(decimal_value='999999999999999999999999999.12345678901234567890123456789',
                  integer_value='99999999999999999999999999999999999998', sign='+0001',
                  token=' A\tB ', language='en-US', name='p:é', ncname='é_', nmtoken='1:a',
                  union_value='+0010', list_value='001\t+02', bool_list='1 0',
                  fixed_text=' done ', default_value='')
    def sample(changes=None, attrs='typed="0 0001 65535"', remove=None):
        """Render one scalar record without deriving expected decisions from generated metadata."""
        selected = dict(fields); selected.update(changes or {})
        return '<sample ' + attrs + '>' + ''.join('<' + key + '>' + text + '</' + key + '>'
                for key, text in selected.items() if key != remove) + '</sample>'
    probes = [('sample', sample(), True)]
    values = {
        'decimal_value': [('-1.0000000000000000000000000000', True), ('-1.0000000000000000000000000001', False),
                          ('999999999999999999999999999.12345678901234567890123456790', False),
                          ('+.5', True), ('1.', True), ('-0.000', True), ('1e0', False), ('NaN', False), ('.', False)],
        'integer_value': [('-99999999999999999999999999999999999999', True),
                          ('-999999999999999999999999999999999999999', False),
                          ('99999999999999999999999999999999999999', False), ('+000', True), ('1.0', False)],
        'sign': [('0', False), ('-0', False), ('-1', False), ('1234567890123456789012345678901234567890', True)],
        'token': [('\t\r\n', False), (' A  B\tC ', True), ('abcdefghijk', False), ('A\u00a0B', True)],
        'language': [('i-klingon', True), ('en-US-123', True), ('abcdefghij', False), ('en_US', False), ('', False)],
        'name': [('1name', False), (':a', True), ('a:b:c', True), ('a b', False)],
        'ncname': [('1name', False), ('a:b', False), ('a.b-1', True), ('', False)],
        'nmtoken': [('1:a-b.c', True), ('a b', False), ('', False)],
        'union_value': [('ready', True), (' pending ', True), ('0 1 255', True), ('+00020', True),
                        ('9', False), ('21', False), ('1 2', False), ('1 2 256', False), ('other', False), ('', False), ('1e1', False)],
        'list_value': [('1 2', True), ('2 1', False), ('1', False), ('1 2 3', False), ('1 65536', False), ('1.0 2', False)],
        'bool_list': [('true false', True), ('true 0', True), ('false true', False), ('True false', False), ('true', False)],
        'fixed_text': [('', True), ('done', True), ('other', False)],
        'default_value': [(' custom ', True), (' ', True)],
    }
    for field, alternatives in values.items():
        probes += [('sample', sample({field: text}), valid) for text, valid in alternatives]
    for attrs, valid in [('typed="0 1 65535" typed_optional="1 false" unit=" cm "', True),
                         ('typed="0 1 65535" typed_optional=""', True),
                         ('typed="0 1 65535" typed_optional="1 0 1"', False),
                         ('typed="0 1 65536"', False), ('typed="0 1"', False),
                         ('typed="0 1 2 3"', False), ('typed=""', False), ('', False),
                         ('typed="0 1 65535" unit=""', False), ('typed="0 1 65535" unit="km"', False)]:
        probes.append(('sample', sample(attrs=attrs), valid))
    for attrs, valid in [('typed="0 1 65535" default_attribute="m" fixed_flag="1"', True),
                         ('typed="0 1 65535" default_attribute=""', False),
                         ('typed="0 1 65535" fixed_flag="0"', False)]:
        probes.append(('sample', sample(attrs=attrs), valid))
    probes += [('sample', sample(remove='default_value'), False), ('sample', sample(remove='fixed_text'), False)]
    for text, attrs, valid in [('9007199254740992', '', True), ('9007199254740993', 'unit="cm"', True),
                               ('9007199254740991', '', False), ('9007199254740994', '', False),
                               ('9007199254740992', 'unit="km"', False), ('', '', False)]:
        probes.append(('amount', '<amount ' + attrs + '>' + text + '</amount>', valid))
    probes += [('words', '<words>é:a 1</words>', True), ('words', '<words>é:a\t1</words>', True),
               ('words', '<words>one</words>', False), ('words', '<words>one two three</words>', False),
               ('words', '<words>one ?</words>', False), ('words', '<words/>', False)]
    probes += [('guard', '<guard><value>fallback</value></guard>', True),
               ('guard-work', '<guard><value>' + 'a' * 20000 + '</value></guard>', False)]
    for text, valid in [('0 65535', True), ('0 65536', False), ('1', False), ('0 1 2', False)]:
        probes.append(('numbers', '<numbers>' + text + '</numbers>', valid))
    probes += [('items', '<items>a b</items>', True), ('items', '<items/>', True),
               ('items-limit', '<items>' + ' '.join(['a'] * 32) + '</items>', False),
               ('items-text', '<items>' + ' '.join(['😀'] * 4) + '</items>', False)]
    for text, valid in [('one two', True), ('one', False), ('one two three', False)]:
        probes.append(('fixed_list', '<fixed_list>' + text + '</fixed_list>', valid))
    for text, valid in [('<values>0 65535</values>', True), ('<values>0 65536</values>', False),
                        ('<values>0</values>', False), ('', False), ('<values>0 1</values><values>0 1</values>', False)]:
        probes.append(('child_list', '<child_list>' + text + '</child_list>', valid))
    for text, valid in [('', True), ('<values/>', True), ('<values>0 65535</values>', True),
                        ('<values>0 65536</values>', False), ('<values>0 1 2</values>', False)]:
        probes.append(('optional_child', '<optional_child>' + text + '</optional_child>', valid))
    for text, valid in [('<id>1</id><values>0 65535</values>', True), ('<values>0 65535</values><id>1</id>', True),
                        ('<values>0 1</values><values>0 1</values><id>1</id>', False)]:
        probes.append(('list_all', '<list_all>' + text + '</list_all>', valid))
    probes += [('plain_default', '<plain_default/>', True), ('plain_default', '<plain_default> </plain_default>', True),
               ('plain_default', '<plain_default>custom</plain_default>', True)]
    return probes

JAVA = '''import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
/** Exercise authoritative scalar contracts using the generated owning APIs. */
public final class Main {
  /** Match the independent oracle and retain canonical XML and ordinary native values. */
  public static void main(String[] args) throws Exception {
    for(String path:args){boolean valid=path.endsWith(".valid.xml");byte[] bytes=Files.readAllBytes(Path.of(path)),encoded;
      try{
        if(Path.of(path).getFileName().toString().startsWith("items")){
          var defaults=Schema.Limits.defaults();boolean limited=path.contains("items-limit"),textLimit=path.contains("items-text");
          var limits=new Schema.Limits(defaults.maxBytes,textLimit?20:defaults.maxStringBytes,limited?16:defaults.maxElements,defaults.maxDepth);
          var candidate=new Schema.Scalars.Items();candidate.value=limited?new String[32]:textLimit?new String[]{"😀","😀","😀","😀"}:new String[]{"a","b"};if(limited)Arrays.fill(candidate.value,"a");
          try{candidate.encodeXml(limits,null);if(!valid)throw new AssertionError("List output resource bound bypassed");}catch(IllegalArgumentException failure){if(valid||!failure.getMessage().contains("limit"))throw failure;}
          var value=Schema.Scalars.Items.decodeXml(bytes,limits,null);encoded=value.encodeXml(limits,null);
        }else if(Path.of(path).getFileName().toString().startsWith("numbers")){
          var value=Schema.Scalars.Numbers.decodeXml(bytes);encoded=value.encodeXml();
        }else if(Path.of(path).getFileName().toString().startsWith("fixed_list")){
          var value=Schema.Scalars.FixedList.decodeXml(bytes);encoded=value.encodeXml();value.value=new String[]{"one"};
          try{value.encodeXml();throw new AssertionError("Native fixed list extent");}catch(IllegalArgumentException expected){}
        }else if(Path.of(path).getFileName().toString().startsWith("child_list")){
          var value=Schema.Scalars.ChildList.decodeXml(bytes);encoded=value.encodeXml();
        }else if(Path.of(path).getFileName().toString().startsWith("optional_child")){
          var value=Schema.Scalars.OptionalChild.decodeXml(bytes);encoded=value.encodeXml();var copy=Schema.Scalars.OptionalChild.decodeXml(encoded);
          if(!Arrays.equals(value.encode(Schema.Protocol.BINARY_NONE),copy.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Optional lexical list presence changed");
        }else if(Path.of(path).getFileName().toString().startsWith("list_all")){
          var value=Schema.Scalars.ListAll.decodeXml(bytes);encoded=value.encodeXml();
        }else if(Path.of(path).getFileName().toString().startsWith("plain_default")){
          var value=Schema.Scalars.PlainDefault.decodeXml(bytes);encoded=value.encodeXml();value.value="";
          try{value.encodeXml();throw new AssertionError("Unrepresentable default value published");}catch(IllegalArgumentException expected){if(!expected.getMessage().equals("Empty XML value would activate a different XSD default"))throw expected;}
        }else if(Path.of(path).getFileName().toString().startsWith("guard")){
          var candidate=new Schema.Scalars.Guard();candidate.value=valid?"fallback":"a".repeat(20000);
          try{candidate.encodeXml();if(!valid)throw new AssertionError("Union resource failure swallowed");}
          catch(IllegalArgumentException failure){if(valid||!failure.getMessage().equals("Portable XML regex work limit exceeded"))throw failure;}
          var value=Schema.Scalars.Guard.decodeXml(bytes);encoded=value.encodeXml();
        }else if(Path.of(path).getFileName().toString().startsWith("amount")){
          var value=Schema.Scalars.Amount.decodeXml(bytes);encoded=value.encodeXml();
          if(!value.unit.isPresent())throw new AssertionError("XSD default missing");
        }else if(Path.of(path).getFileName().toString().startsWith("words")){
          var value=Schema.Scalars.Words.decodeXml(bytes);encoded=value.encodeXml();
          if(value.value.length!=2)throw new AssertionError("Typed text list");
          value.value[0]="two words";try{value.encodeXml();throw new AssertionError("Ambiguous list item");}catch(IllegalArgumentException expected){}
        }else{
          var value=Schema.Scalars.Sample.decodeXml(bytes);encoded=value.encodeXml();var copy=Schema.Scalars.Sample.decodeXml(encoded);
          if(!Arrays.equals(value.encode(Schema.Protocol.BINARY_NONE),copy.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Native wire changed: "+path);
          if(value.typed.length!=3||!value.unit.isPresent()||value.defaultAttribute.isEmpty()||!value.fixedFlag)throw new AssertionError("Typed list/default");
          for(int index=0;index<7;++index){var broken=Schema.Scalars.Sample.decodeXml(encoded);
            switch(index){case 0:broken.decimalValue="1e0";break;case 1:broken.integerValue="1.0";break;case 2:broken.unionValue="unknown";break;case 3:broken.listValue="2 1";break;case 4:broken.fixedText="bad";break;case 5:broken.typed=new short[2];break;default:broken.sign="0";}
            try{broken.encodeXml();throw new AssertionError("Invalid scalar output published");}catch(IllegalArgumentException expected){}
          }
        }
      }catch(IllegalArgumentException failure){if(valid)throw new AssertionError(path,failure);if(Path.of(path).getFileName().toString().startsWith("guard")&&!failure.getMessage().equals("Portable XML regex work limit exceeded"))throw failure;continue;}
      if(!valid)throw new AssertionError("Invalid scalar input published: "+path);Files.write(Path.of(path+".java.xml"),encoded);
    }
    System.out.println("Java exact decimal/integer, lists/unions, defaults/fixed and simpleContent pass");
  }
}'''

CSHARP = '''using System;
using System.IO;
using static SerializerGenerated.Schema;
// Exercise authoritative scalar contracts using generated owning APIs.
internal static class Program {
  // Match the independent oracle before publishing a complete candidate or output.
  private static void Main(string[] args){foreach(string path in args){bool valid=path.EndsWith(".valid.xml",StringComparison.Ordinal);byte[] bytes=File.ReadAllBytes(path),encoded;
    try{
      if(Path.GetFileName(path).StartsWith("items",StringComparison.Ordinal)){
        var defaults=new Limits();bool limited=path.Contains("items-limit",StringComparison.Ordinal),textLimit=path.Contains("items-text",StringComparison.Ordinal);
        var limits=new Limits(defaults.MaxBytes,textLimit?20:defaults.MaxStringBytes,limited?16:defaults.MaxElements,defaults.MaxDepth);
        var candidate=new ScalarsItems{Value=limited?new System.Collections.Generic.List<string>(new string[32]):textLimit?new System.Collections.Generic.List<string>{"😀","😀","😀","😀"}:new System.Collections.Generic.List<string>{"a","b"}};if(limited)for(int index=0;index<32;++index)candidate.Value[index]="a";
        try{candidate.EncodeXml(limits,null);if(!valid)throw new Exception("List output resource bound bypassed");}catch(FormatException failure){if(valid||!failure.Message.Contains("limit",StringComparison.Ordinal))throw;}
        var value=ScalarsItems.DecodeXml(bytes,limits,null);encoded=value.EncodeXml(limits,null);
      }else if(Path.GetFileName(path).StartsWith("numbers",StringComparison.Ordinal)){
        var value=ScalarsNumbers.DecodeXml(bytes);encoded=value.EncodeXml();
      }else if(Path.GetFileName(path).StartsWith("fixed_list",StringComparison.Ordinal)){
        var value=ScalarsFixedList.DecodeXml(bytes);encoded=value.EncodeXml();value.Value=new System.Collections.Generic.List<string>{"one"};
        try{value.EncodeXml();throw new Exception("Native fixed list extent");}catch(FormatException){}
      }else if(Path.GetFileName(path).StartsWith("child_list",StringComparison.Ordinal)){
        var value=ScalarsChildList.DecodeXml(bytes);encoded=value.EncodeXml();
      }else if(Path.GetFileName(path).StartsWith("optional_child",StringComparison.Ordinal)){
        var value=ScalarsOptionalChild.DecodeXml(bytes);encoded=value.EncodeXml();var copy=ScalarsOptionalChild.DecodeXml(encoded);
        if(!value.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(copy.Encode(Protocol.BINARY_NONE)))throw new Exception("Optional lexical list presence changed");
      }else if(Path.GetFileName(path).StartsWith("list_all",StringComparison.Ordinal)){
        var value=ScalarsListAll.DecodeXml(bytes);encoded=value.EncodeXml();
      }else if(Path.GetFileName(path).StartsWith("plain_default",StringComparison.Ordinal)){
        var value=ScalarsPlainDefault.DecodeXml(bytes);encoded=value.EncodeXml();value.Value="";
        try{value.EncodeXml();throw new Exception("Unrepresentable default value published");}catch(FormatException expected){if(expected.Message!="Empty XML value would activate a different XSD default")throw;}
      }else if(Path.GetFileName(path).StartsWith("guard",StringComparison.Ordinal)){
        var candidate=new ScalarsGuard{Value=valid?"fallback":new string('a',20000)};
        try{candidate.EncodeXml();if(!valid)throw new Exception("Union resource failure swallowed");}
        catch(FormatException failure){if(valid||failure.Message!="Portable XML regex work limit exceeded")throw;}
        var value=ScalarsGuard.DecodeXml(bytes);encoded=value.EncodeXml();
      }else if(Path.GetFileName(path).StartsWith("amount",StringComparison.Ordinal)){
        var value=ScalarsAmount.DecodeXml(bytes);encoded=value.EncodeXml();if(!value.Unit.HasValue)throw new Exception("XSD default missing");
      }else if(Path.GetFileName(path).StartsWith("words",StringComparison.Ordinal)){
        var value=ScalarsWords.DecodeXml(bytes);encoded=value.EncodeXml();if(value.Value.Count!=2)throw new Exception("Typed text list");
        value.Value[0]="two words";try{value.EncodeXml();throw new Exception("Ambiguous list item");}catch(FormatException){}
      }else{
        var value=ScalarsSample.DecodeXml(bytes);encoded=value.EncodeXml();var copy=ScalarsSample.DecodeXml(encoded);
        if(!value.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(copy.Encode(Protocol.BINARY_NONE)))throw new Exception("Native wire changed: "+path);
        if(value.Typed.Count!=3||!value.Unit.HasValue||value.DefaultAttribute.Length==0||!value.FixedFlag)throw new Exception("Typed list/default");
        for(int index=0;index<7;++index){var broken=ScalarsSample.DecodeXml(encoded);
          switch(index){case 0:broken.DecimalValue="1e0";break;case 1:broken.IntegerValue="1.0";break;case 2:broken.UnionValue="unknown";break;case 3:broken.ListValue="2 1";break;case 4:broken.FixedText="bad";break;case 5:broken.Typed=new System.Collections.Generic.List<ushort>{1,2};break;default:broken.Sign="0";break;}
          try{broken.EncodeXml();throw new Exception("Invalid scalar output published");}catch(FormatException){}
        }
      }
    }catch(FormatException failure){if(valid)throw new Exception(path,failure);if(Path.GetFileName(path).StartsWith("guard",StringComparison.Ordinal)&&failure.Message!="Portable XML regex work limit exceeded")throw;continue;}
    if(!valid)throw new Exception("Invalid scalar input published: "+path);File.WriteAllBytes(path+".csharp.xml",encoded);
  }Console.WriteLine("C# exact decimal/integer, lists/unions, defaults/fixed and simpleContent pass");}
}'''

GO = r'''package main
import("bytes";"fmt";"os";"path/filepath";"strings")
// Require staged output failure without a candidate byte slice.
func rejected(data []byte,err error){if data!=nil||err==nil{panic("Invalid scalar output published")}}
// Exercise complete generated native mappings for one independent input fixture.
func probe(path string)([]byte,error){
  data,err:=os.ReadFile(path);if err!=nil{panic(err)};file:=filepath.Base(path);valid:=strings.HasSuffix(path,".valid.xml")
  switch{
  case strings.HasPrefix(file,"items"):
    policy:=DefaultXmlOptions();limited,textLimit:=strings.HasPrefix(file,"items-limit"),strings.HasPrefix(file,"items-text")
    if limited{policy.Limits.MaxNodes=16};if textLimit{policy.Limits.MaxTextBytes=20}
    candidate:=NewScalarsItems();candidate.Value=[]string{"a","b"};if limited{candidate.Value=make([]string,32);for index:=range candidate.Value{candidate.Value[index]="a"}};if textLimit{candidate.Value=[]string{"😀","😀","😀","😀"}}
    output,err:=candidate.EncodeXML(policy);if !valid{rejected(output,err);if !strings.Contains(err.Error(),"limit"){panic(err)}}else if err!=nil{panic(err)}
    value,err:=DecodeScalarsItemsXML(data,policy);if err!=nil{return nil,err};return value.EncodeXML(policy)
  case strings.HasPrefix(file,"numbers"):
    value,err:=DecodeScalarsNumbersXML(data);if err!=nil{return nil,err};return value.EncodeXML()
  case strings.HasPrefix(file,"fixed_list"):
    value,err:=DecodeScalarsFixedListXML(data);if err!=nil{return nil,err};output,err:=value.EncodeXML();if err!=nil{return nil,err};value.Value=[]string{"one"};rejected(value.EncodeXML());return output,nil
  case strings.HasPrefix(file,"child_list"):
    value,err:=DecodeScalarsChildListXML(data);if err!=nil{return nil,err};return value.EncodeXML()
  case strings.HasPrefix(file,"optional_child"):
    value,err:=DecodeScalarsOptionalChildXML(data);if err!=nil{return nil,err};output,err:=value.EncodeXML();if err!=nil{return nil,err};copy,err:=DecodeScalarsOptionalChildXML(output);if err!=nil{return nil,err};first,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};second,err:=copy.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(first,second){panic("Optional list presence changed")};return output,nil
  case strings.HasPrefix(file,"list_all"):
    value,err:=DecodeScalarsListAllXML(data);if err!=nil{return nil,err};return value.EncodeXML()
  case strings.HasPrefix(file,"plain_default"):
    value,err:=DecodeScalarsPlainDefaultXML(data);if err!=nil{return nil,err};output,err:=value.EncodeXML();if err!=nil{return nil,err};value.Value="";failed,failure:=value.EncodeXML();rejected(failed,failure);if failure.Error()!="Empty XML value would activate a different XSD default"{panic(failure)};return output,nil
  case strings.HasPrefix(file,"guard"):
    candidate:=NewScalarsGuard();candidate.Value="fallback";if !valid{candidate.Value=strings.Repeat("a",20000)};output,err:=candidate.EncodeXML();if !valid{rejected(output,err);if err.Error()!="Portable XML regex work limit exceeded"{panic(err)}}else if err!=nil{panic(err)}
    value,err:=DecodeScalarsGuardXML(data);if err!=nil{return nil,err};return value.EncodeXML()
  case strings.HasPrefix(file,"amount"):
    value,err:=DecodeScalarsAmountXML(data);if err!=nil{return nil,err};if value.Unit==nil{panic("XSD default missing")};return value.EncodeXML()
  case strings.HasPrefix(file,"words"):
    value,err:=DecodeScalarsWordsXML(data);if err!=nil{return nil,err};output,err:=value.EncodeXML();if err!=nil{return nil,err};if len(value.Value)!=2{panic("Typed text list")};value.Value[0]="two words";rejected(value.EncodeXML());return output,nil
  }
  value,err:=DecodeScalarsSampleXML(data);if err!=nil{return nil,err};output,err:=value.EncodeXML();if err!=nil{return nil,err};copy,err:=DecodeScalarsSampleXML(output);if err!=nil{return nil,err}
  first,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};second,err:=copy.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(first,second){panic("Native wire changed: "+path)}
  if len(value.Typed)!=3||value.Unit==nil||value.DefaultAttribute==""||!value.FixedFlag{panic("Typed list/default")}
  for index:=0;index<7;index++{broken,err:=DecodeScalarsSampleXML(output);if err!=nil{panic(err)};switch index{case 0:broken.DecimalValue="1e0";case 1:broken.IntegerValue="1.0";case 2:broken.UnionValue="unknown";case 3:broken.ListValue="2 1";case 4:broken.FixedText="bad";case 5:broken.Typed=[]uint16{1,2};default:broken.Sign="0"};rejected(broken.EncodeXML())}
  return output,nil
}
// Compare SDK results with independently named fixtures before writing any successful output.
func main(){for _,path:=range os.Args[1:]{valid:=strings.HasSuffix(path,".valid.xml");output,err:=probe(path);if valid!=(err==nil)||!valid&&output!=nil{panic(fmt.Sprintf("Oracle differs: %s: %v",path,err))};if err!=nil{if strings.HasPrefix(filepath.Base(path),"guard")&&err.Error()!="Portable XML regex work limit exceeded"{panic(err)};continue};if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}};fmt.Println("Go exact decimal/integer, lists/unions, defaults/fixed and simpleContent pass")}
'''


def rejected_bindings(args, languages):
    """Accept supported identities and protect existing files when a selected contract is unsupported."""
    directory = args.build / 'rejections'; directory.mkdir(parents=True, exist_ok=True)
    fixtures = [
        ('<xs:element name="value" default="p:item"><xs:simpleType><xs:union memberTypes="xs:QName xs:unsignedShort"/></xs:simpleType></xs:element>', '', 'QName union defaults'),
        ('<xs:element name="value"><xs:simpleType><xs:restriction><xs:simpleType><xs:list itemType="xs:QName"/></xs:simpleType><xs:enumeration value="p:item"/></xs:restriction></xs:simpleType></xs:element>', '', 'QName list enumeration'),
        ('<xs:element name="value" fixed="1"><xs:simpleType><xs:union memberTypes="xs:boolean xs:unsignedShort"/></xs:simpleType></xs:element>', '', 'union fixed values'),
        ('<xs:element name="value"><xs:simpleType><xs:restriction><xs:simpleType><xs:union memberTypes="xs:boolean xs:unsignedShort"/></xs:simpleType><xs:enumeration value="1"/></xs:restriction></xs:simpleType></xs:element>', '', 'union enumeration'),
        ('<xs:element name="value"><xs:simpleType><xs:list itemType="Item"/></xs:simpleType></xs:element>', '<xs:simpleType name="Item"><xs:restriction base="xs:unsignedShort"><xs:maxInclusive value="9"/></xs:restriction></xs:simpleType>', 'list item restrictions'),
    ]
    model = directory / 'model.serializer'
    model.write_text('serializer version 1.20.0; namespace scalars { using schema "scalars.xsd"; class sample { public string value; } }', encoding='utf-8')
    for index, (element, types, diagnostic) in enumerate(fixtures):
        xsd = '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:p="urn:items">' + types + '<xs:element name="sample"><xs:complexType><xs:sequence>' + element + '</xs:sequence></xs:complexType></xs:element></xs:schema>'
        (directory / 'scalars.xsd').write_text(xsd, encoding='utf-8')
        command = [str(args.compiler), '--input', str(model), '--language', ','.join(languages)]
        outputs = []
        for language in languages:
            output = directory / sdk.OUTPUTS[language]; output.write_text('preserve existing output', encoding='utf-8'); outputs.append(output)
            command += [f'--{language}.output', str(output)]
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=60)
        if diagnostic in ('QName union defaults', 'QName list enumeration', 'union fixed values', 'union enumeration', 'list item restrictions'):
            if result.returncode != 0 or any(path.read_text(encoding='utf-8') == 'preserve existing output' for path in outputs):
                raise AssertionError(f'Supported QName enumeration failed: {result.stdout} {result.stderr}')
            continue
        if result.returncode == 0 or diagnostic not in result.stdout + result.stderr or any(path.read_text(encoding='utf-8') != 'preserve existing output' for path in outputs):
            raise AssertionError(f'Atomic scalar gate {index} failed: {result.stdout} {result.stderr}')
    return len(fixtures)


def main():
    """Generate through the real compiler, execute native SDKs, and independently validate output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='java,csharp,go')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler = args.compiler.resolve(); args.build = args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(language not in ('java', 'csharp', 'go') for language in languages):
        parser.error('Select distinct language names from java,csharp,go')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    (args.build / 'scalars.xsd').write_text(XSD, encoding='utf-8')
    oracle = oracle_schema(); paths = []
    for index, (kind, document, valid) in enumerate(cases()):
        if independent(document.encode(), oracle) != (valid or kind in ('guard-work', 'items-limit', 'items-text')):
            raise AssertionError(f'Independent XSD decision differs for {index}: {oracle.error_log}')
        path = args.build / f'{kind}-{index}.{"valid" if valid else "invalid"}.xml'; path.write_text(document, encoding='utf-8'); paths.append(path)
    model = args.build / 'model.serializer'; model.write_text(MODEL, encoding='utf-8')
    command = [args.compiler, '--input', model, '--language', ','.join(languages), '--protocols', 'binary_none', '--go.package', 'main']
    for language in languages:
        target = args.build / language; target.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    for language, filename, source in [('java', 'Main.java', JAVA), ('csharp', 'Program.cs', CSHARP), ('go', 'main.go', GO)]:
        if language not in languages:
            continue
        directory = args.build / 'source' / language; directory.mkdir(parents=True, exist_ok=True)
        (directory / filename).write_text(source, encoding='utf-8')
        for path in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
        launch = runner.build(language, 'xml_scalars', args.build / language, directory)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path in paths:
            emitted = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                if not independent(emitted.read_bytes(), oracle):
                    raise AssertionError(f'Independent output oracle differs: {emitted}: {oracle.error_log}')
            elif emitted.exists():
                raise AssertionError(f'Invalid scalar output published: {emitted}')
    binding_checks = rejected_bindings(args, languages)
    print(f'PASS: {len(paths)} independent exact scalar/list/union/default/simpleContent probes and {binding_checks} binding checks across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
