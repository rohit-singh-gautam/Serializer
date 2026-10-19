"""Qualify generated nested, optional, numeric, enum and fixed-array XML mappings."""
from schema import Protocol, ShapesRecord, ShapesChild, ShapesState, ShapesPrice, ShapesCatalog, ShapesIntegerBox, ShapesTextBox, ShapesNames, XmlSchema


def rejected(document):
    """Require invalid scalar lexical values to fail before publishing a model."""
    try:
        ShapesRecord.decode_xml(document)
    except (ValueError, OverflowError):
        return
    raise AssertionError('Invalid XML scalar accepted')


def main():
    """Check complete model values and selected prepared runtime XSD enforcement."""
    names_xml = b"<r:names xmlns:r='urn:root' xmlns:a='urn:attribute' xml:lang='en' a:qualified='A&amp;B'><unqualified>plain</unqualified><f:foreign xmlns:f='urn:foreign'>foreign</f:foreign></r:names>"
    names = ShapesNames.decode_xml(names_xml)
    assert (names.language, names.qualified, names.unqualified, names.foreign) == ('en', 'A&B', 'plain', 'foreign')
    assert ShapesNames.decode_xml(names.encode_xml()).encode(Protocol.BINARY_NONE) == names.encode(Protocol.BINARY_NONE)
    integer_box = ShapesIntegerBox.decode_xml(b'<integer_box><value>4294967295</value></integer_box>')
    assert integer_box.value == 4294967295
    text_box = ShapesTextBox.decode_xml(b'<text_box><value>A&amp;B</value></text_box>')
    assert text_box.value == 'A&B'
    assert ShapesTextBox.decode_xml(text_box.encode_xml()).value == text_box.value
    document = b"<catalog xmlns='urn:serializer:vocabulary'><prices><price currency='USD'>12.5</price><price currency='EUR'>0</price></prices><labels><label>A&amp;B</label><label/></labels></catalog>"
    catalog = ShapesCatalog.decode_xml(document)
    assert catalog.prices[0].currency == 'USD' and catalog.prices[0].value == 12.5
    assert catalog.labels == ['A&B', '']
    assert ShapesCatalog.decode_xml(catalog.encode_xml()).encode(Protocol.BINARY_NONE) == catalog.encode(Protocol.BINARY_NONE)
    assert ShapesCatalog.decode_xml(ShapesCatalog().encode_xml()).prices == []
    for invalid in (document.replace(b'<prices>', b'<prices extra="1">'),
                    document.replace(b'<prices>', b'<prices/><prices>'),
                    document.replace(b'<labels>', b'<unknown/><labels>'),
                    document.replace(b'>12.5<', b'><value>12.5</value><'),
                    document.replace(b'<label/>', b'<label/><price/>')):
        try:
            ShapesCatalog.decode_xml(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError('Invalid wrapper/simple content accepted')
    value = ShapesRecord()
    value.big = (1 << 64) - 1
    value.label = '\tX\r\n\U0001f600'
    value.flag = (True,)
    value.nested.value = -32768
    child = ShapesChild()
    child.value = 32767
    value.children = [child]
    value.states = [ShapesState.Zero, ShapesState.One]
    value.amount, value.small = float('inf'), 1.25
    encoded = value.encode_xml()
    copy = ShapesRecord.decode_xml(encoded)
    assert copy.encode(Protocol.BINARY_NONE) == value.encode(Protocol.BINARY_NONE)
    value.flag = (False,)
    assert ShapesRecord.decode_xml(value.encode_xml()).flag == (False,)
    value.flag = ()
    assert ShapesRecord.decode_xml(value.encode_xml()).flag == ()
    zero = encoded.replace(b'big="18446744073709551615"', b'big="-000"')
    assert ShapesRecord.decode_xml(zero).big == 0
    for invalid in (b'-', b'18446744073709551616', '\u00a07'.encode()):
        changed = encoded.replace(b'big="18446744073709551615"', b'big="' + invalid + b'"')
        assert changed != encoded
        rejected(changed)
    rejected(encoded.replace(b'flag="true"', b'flag="TRUE"'))
    rejected(encoded.replace(b'>1.25<', b'>1e39<'))
    xsd = b"<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='record' type='xs:string'/></xs:schema>"
    validator = XmlSchema(xsd)
    try:
        ShapesRecord.decode_xml(encoded, schema=validator)
    except ValueError:
        pass
    else:
        raise AssertionError('Runtime XSD was ignored')
    try:
        value.encode_xml(schema=validator)
    except ValueError:
        pass
    else:
        raise AssertionError('Output XSD was ignored')
    print('Python generated typed mapping and runtime XSD checks passed')


if __name__ == '__main__':
    main()
