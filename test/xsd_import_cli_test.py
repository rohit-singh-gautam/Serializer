# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check real XSD import publication, dependencies and optional authored binding."""
from pathlib import Path
import subprocess
import sys
import tempfile


def invoke(compiler, *arguments, success=True):
    """Run one command and retain diagnostics for an unexpected exit status."""
    result = subprocess.run([compiler, *map(str, arguments)], capture_output=True, text=True)
    if (result.returncode == 0) != success:
        raise AssertionError(result.stdout + result.stderr)
    return result


def main():
    """Import twice, protect manual edits and compile both authoring workflows."""
    compiler = str(Path(sys.argv[1]).resolve())
    scratch = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else Path(compiler).parent
    with tempfile.TemporaryDirectory(prefix="serializer-xsd-cli-", dir=scratch) as temporary:
        root = Path(temporary)
        schemas = root / "schemas"
        generated = root / "generated"
        schemas.mkdir()
        generated.mkdir()
        xsd = schemas / "order.xsd"
        xsd.write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
          targetNamespace="urn:order" elementFormDefault="qualified">
          <xs:element name="order"><xs:complexType>
            <xs:sequence><xs:element name="label" type="xs:string"/></xs:sequence>
            <xs:attribute name="id" type="xs:unsignedInt" use="required"/>
          </xs:complexType></xs:element></xs:schema>''', encoding="utf-8")
        source = generated / "order.serializer"
        depfile = generated / "order.serializer.d"
        command = ("--import-xsd", xsd, "--namespace", "orders", "--root", "order",
                   "--output", source, "--depfile", depfile)
        invoke(compiler, *command)
        first = source.read_bytes()
        assert b"serializer version 1.20.0;" in first
        assert b'using schema "../schemas/order.xsd";' in first
        assert "order.xsd" in depfile.read_text(encoding="utf-8")
        invoke(compiler, *command)
        assert source.read_bytes() == first
        header = generated / "order.hpp"
        invoke(compiler, "--input", source, "--language", "cpp", "--output", header,
               "--cpp.format", "false", "--depfile", generated / "order.hpp.d")
        assert "order.xsd" in (generated / "order.hpp.d").read_text(encoding="utf-8")
        source.write_bytes(first + b"\n// application behavior\n")
        previous = source.read_bytes()
        invoke(compiler, *command, success=False)
        assert source.read_bytes() == previous
        authored = schemas / "manual.serializer"
        authored.write_text('''serializer version 1.20.0;
          namespace orders {
            using schema "order.xsd";
            class order { public string label (7); public uint32 id (3); }
          }''', encoding="utf-8")
        invoke(compiler, "--input", authored, "--language", "python", "--output",
               generated / "manual.py")
        invoke(compiler, "--import-xsd", xsd, "--input", authored,
               "--output", generated / "invalid.serializer", success=False)
        assert not (generated / "invalid.serializer").exists()
    print("XSD CLI import, edit protection, relative binding and dependencies passed")


if __name__ == "__main__":
    main()
