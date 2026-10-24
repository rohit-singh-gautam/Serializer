# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Compare the approved fragment subset with Python's independent HTML parser."""

import argparse
from html.parser import HTMLParser
import subprocess


class Trace(HTMLParser):
    """Normalize independent HTML callbacks to paired void tags and adjacent text."""

    def __init__(self):
        """Start one independent parser with an empty normalized trace."""
        super().__init__(convert_charrefs=True)
        self.events = []

    def handle_starttag(self, tag, attrs):
        """Retain ordinary attributes and complete the approved HTML void elements immediately."""
        self.events.append(("start", tag, tuple((key, None if key == "hidden" else value) for key, value in attrs)))
        if tag in {"br", "hr", "img"}:
            self.events.append(("end", tag))

    def handle_startendtag(self, tag, attrs):
        """Normalize the parser's syntactic slash callback to the same semantic trace."""
        self.handle_starttag(tag, attrs)
        if tag not in {"br", "hr", "img"}:
            self.handle_endtag(tag)

    def handle_endtag(self, tag):
        """Retain one explicit nonvoid end tag from the independent parser."""
        self.events.append(("end", tag))

    def handle_data(self, text):
        """Join adjacent callback chunks so entity callback boundaries do not affect comparison."""
        if self.events and self.events[-1][0] == "text":
            self.events[-1] = ("text", self.events[-1][1] + text)
        elif text:
            self.events.append(("text", text))


def trace(text):
    """Apply HTML input line-ending normalization before independent tokenization."""
    parser = Trace()
    parser.feed(text.replace("\r\n", "\n").replace("\r", "\n"))
    parser.close()
    return parser.events


def tree_trace(text):
    """Retain an independent libxml2 HTML fragment tree, including mixed-content tails."""
    from lxml import html

    # Unicode-entry HTML providers do not necessarily perform the byte decoder's line normalization.
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    parent = html.fragment_fromstring(text, create_parent="div")
    events = []

    def add_text(value):
        """Coalesce text across omitted comment nodes while preserving whitespace and tails."""
        if value:
            if events and events[-1][0] == "text":
                events[-1] = ("text", events[-1][1] + value)
            else:
                events.append(("text", value))

    def visit(node):
        """Traverse element identity, attributes and exact mixed-content order without comments."""
        attrs = tuple(sorted((key, None if key == "hidden" else value) for key, value in node.attrib.items()))
        events.append(("start", node.tag, attrs))
        add_text(node.text)
        for child in node:
            if isinstance(child.tag, str):
                visit(child)
            add_text(child.tail)
        events.append(("end", node.tag))

    visit(parent)
    return events


def main():
    """Require every supported fixture to preserve the independent parser's semantic trace."""
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--tree-oracle", action="store_true",
                        help="Also require the independently installed lxml HTML fragment-tree oracle")
    options = parser.parse_args()
    fixtures = [
        '<P>H<SUB title="index">2</SUB>O; x<SUP>2<em>nd</em></SUP> &amp; y<sub><sup>n</sup></sub></P>',
        "<DIV hidden class=card><P>A&nbsp;&amp; B<br><IMG src='x.png' alt=picture></P></DIV>",
        '<section><h1 title="A &quot;quote&quot;">Title</h1><p>Unicode: \u03bb \U0001f642</p></section>',
        '<ul>\n<li>first <b>bold</b></li><li><a href="a?x=1&amp;y=2">link</a></li></ul>',
        'prefix<!-- discarded --><span hidden=hidden title="&#x1F642;">x\r\ny&#13;&lt;</span>suffix',
        '<article><hr/><p><em>one</em><br />two</p><blockquote>  literal\nspace</blockquote></article>',
        '<img src=x/ alt="a &gt; b">',
    ]
    for source in fixtures:
        result = subprocess.run([options.binary], input=source.encode("utf-8"), check=True, capture_output=True)
        actual = result.stdout.decode("utf-8")
        if trace(source) != trace(actual):
            raise AssertionError((source, actual, trace(source), trace(actual)))
        if options.tree_oracle and tree_trace(source) != tree_trace(actual):
            raise AssertionError((source, actual, tree_trace(source), tree_trace(actual)))
        repeat = subprocess.run([options.binary], input=actual.encode("utf-8"), check=True, capture_output=True).stdout.decode("utf-8")
        if repeat != actual:
            raise AssertionError("HTML canonical output is not stable")
    provider = "HTMLParser and lxml HTML trees" if options.tree_oracle else "HTMLParser"
    print(f"Independent {provider} traces and canonical replay agree for {len(fixtures)} HTML fragments")


if __name__ == "__main__":
    main()
