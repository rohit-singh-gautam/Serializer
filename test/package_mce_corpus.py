"""Independent ECMA-376 Part 3 fifth-edition Relationships processing decisions."""
import python_package_test as independent
import xml.etree.ElementTree as etree

MC = independent.MC
RELS = independent.RELS
MAIN = '<Relationship Id="main" Type="urn:serializer:main-model" Target="Alternate/root.xml"/>'
START = f'<Relationships xmlns="{RELS}" xmlns:mc="{MC}" xmlns:r="{RELS}" xmlns:e="urn:vendor"'


def cases():
    """Author complete OPC packages whose decisions follow the normative reference Steps 1–3."""
    rows = [
        ('ignored-attributes', ' mc:Ignorable="e" e:flag="ignored">' + MAIN, True),
        ('ignored-subtree', ' mc:Ignorable="e"><e:drop mc:MustUnderstand="e"><Unknown/></e:drop>' + MAIN, True),
        ('process-exact', ' mc:Ignorable="e" mc:ProcessContent="e:box"><e:box>' + MAIN + '</e:box>', True),
        ('process-wildcard', ' mc:Ignorable="e" mc:ProcessContent="e:*"><e:box>' + MAIN + '</e:box>', True),
        ('understood-ignorable', ' mc:Ignorable="r">' + MAIN, True),
        ('must-understood', ' mc:MustUnderstand="r">' + MAIN, True),
        ('empty-directives', ' mc:Ignorable="" mc:ProcessContent="" mc:MustUnderstand="">' + MAIN, True),
        ('prefix-alias', ' xmlns:f="urn:vendor" mc:Ignorable="e" mc:ProcessContent="e:box"><f:box>' + MAIN + '</f:box>', True),
        ('prefix-rebinding', ' xmlns:f="urn:vendor" mc:Ignorable="e" mc:ProcessContent="e:box"><f:box xmlns:e="urn:other">' + MAIN + '</f:box>', True),
        ('choice-first', '><mc:AlternateContent><mc:Choice Requires="r">' + MAIN + '</mc:Choice><mc:Choice Requires="r"><Unknown/></mc:Choice><mc:Fallback><Unknown/></mc:Fallback></mc:AlternateContent>', True),
        ('choice-fallback', '><mc:AlternateContent><mc:Choice Requires="e" mc:MustUnderstand="e"><Unknown/></mc:Choice><mc:Fallback>' + MAIN + '</mc:Fallback></mc:AlternateContent>', True),
        ('choice-second', '><mc:AlternateContent><mc:Choice Requires="e"><Unknown/></mc:Choice><mc:Choice Requires="r">' + MAIN + '</mc:Choice></mc:AlternateContent>', True),
        ('choice-ignored-sibling', '><mc:AlternateContent mc:Ignorable="e"><e:future/><mc:Choice Requires="r">' + MAIN + '</mc:Choice><e:future/></mc:AlternateContent>', True),
        ('choice-no-selected', '><mc:AlternateContent><mc:Choice Requires="e"><Unknown/></mc:Choice></mc:AlternateContent>' + MAIN, True),
        ('ignored-attribute-child', ' mc:Ignorable="e">' + MAIN.replace('/>', ' e:flag="ignored"/>'), True),
        ('foreign-core', '><e:Unknown/>' + MAIN, False),
        ('unknown-core', '><Unknown/>' + MAIN, False),
        ('unknown-attribute', ' unknown="bad">' + MAIN, False),
        ('unbound-prefix', ' mc:Ignorable="missing">' + MAIN, False),
        ('compatibility-prefix', ' mc:Ignorable="mc">' + MAIN, False),
        ('must-unsupported', ' mc:MustUnderstand="e">' + MAIN, False),
        ('must-unbound', ' mc:MustUnderstand="missing">' + MAIN, False),
        ('process-not-ignorable', ' mc:ProcessContent="e:box">' + MAIN, False),
        ('process-no-prefix', ' mc:Ignorable="e" mc:ProcessContent="box">' + MAIN, False),
        ('process-bad-local', ' mc:Ignorable="e" mc:ProcessContent="e:a:b">' + MAIN, False),
        ('process-context', ' mc:Ignorable="e" mc:ProcessContent="e:*"><e:box xml:lang="en">' + MAIN + '</e:box>', False),
        ('unknown-directive', ' mc:Unknown="e">' + MAIN, False),
        ('legacy-preserve', ' mc:Ignorable="e" mc:PreserveElements="e:*">' + MAIN, False),
        ('xml-whitespace-only', ' mc:Ignorable="e&#xA0;r">' + MAIN, False),
        ('unexpected-choice', '><mc:Choice Requires="r">' + MAIN + '</mc:Choice>', False),
        ('empty-alternate', '><mc:AlternateContent/>' + MAIN, False),
        ('missing-requires', '><mc:AlternateContent><mc:Choice>' + MAIN + '</mc:Choice></mc:AlternateContent>', False),
        ('empty-requires', '><mc:AlternateContent><mc:Choice Requires=" "/><mc:Fallback>' + MAIN + '</mc:Fallback></mc:AlternateContent>', False),
        ('fallback-first', '><mc:AlternateContent><mc:Fallback/><mc:Choice Requires="r">' + MAIN + '</mc:Choice></mc:AlternateContent>', False),
        ('duplicate-fallback', '><mc:AlternateContent><mc:Choice Requires="e"/><mc:Fallback>' + MAIN + '</mc:Fallback><mc:Fallback/></mc:AlternateContent>', False),
        ('choice-after-fallback', '><mc:AlternateContent><mc:Choice Requires="e"/><mc:Fallback>' + MAIN + '</mc:Fallback><mc:Choice Requires="r"/></mc:AlternateContent>', False),
        ('alternate-text', '>text<mc:AlternateContent><mc:Choice Requires="r">' + MAIN + '</mc:Choice></mc:AlternateContent>', False),
        ('unwrapped-text', ' mc:Ignorable="e" mc:ProcessContent="e:*"><e:box>text' + MAIN + '</e:box>', False),
        ('wrapper-xml-context', '><mc:AlternateContent xml:space="preserve"><mc:Choice Requires="r">' + MAIN + '</mc:Choice></mc:AlternateContent>', False),
        ('unselected-wrapper-attribute', '><mc:AlternateContent><mc:Choice Requires="r">' + MAIN + '</mc:Choice><mc:Choice Requires="e" unknown="bad"/></mc:AlternateContent>', False),
    ]
    for name, body, accepted in rows:
        parts = independent.independent_parts()
        parts['_rels/.rels'] = (START + body + '</Relationships>').encode('utf-8')
        etree.fromstring(parts['_rels/.rels'])
        yield 'mce-' + name, independent.independent_zip(parts), accepted
    root = f'<mc:AlternateContent xmlns:mc="{MC}" xmlns:r="{RELS}" xmlns:e="urn:vendor"><mc:Choice Requires="e"><Unknown/></mc:Choice><mc:Fallback><r:Relationships>' + MAIN.replace('<Relationship', '<r:Relationship') + '</r:Relationships></mc:Fallback></mc:AlternateContent>'
    for suffix, text, accepted in (('root-selected', root, True),
                                   ('root-empty', root.replace('Requires="e"', 'Requires="e"').replace('<mc:Fallback>', '<mc:Choice Requires="e">').replace('</mc:Fallback>', '</mc:Choice>'), False),
                                   ('root-multiple', root.replace('</r:Relationships>', '</r:Relationships><r:Relationships/>'), False)):
        parts = independent.independent_parts(); parts['_rels/.rels'] = text.encode('utf-8')
        etree.fromstring(parts['_rels/.rels'])
        yield 'mce-' + suffix, independent.independent_zip(parts), accepted
