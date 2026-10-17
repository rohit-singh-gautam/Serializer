#!/usr/bin/env python3
"""Exercise fixed-XSD CLI tool modes, depfiles, identity pins and publication failures."""
from __future__ import annotations

import hashlib
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as element_tree


def run(compiler: Path, source: Path, *arguments: str, success: bool = True) -> str:
    """Run the actual compiler and require the selected success/failure outcome."""
    result = subprocess.run(
        [str(compiler), "--input", str(source), "--xml.enabled", "true", *arguments],
        capture_output=True, text=True, check=False,
    )
    if (result.returncode == 0) != success:
        raise AssertionError(f"Unexpected compiler exit {result.returncode}:\n{result.stdout}\n{result.stderr}")
    return result.stdout + result.stderr


def main() -> None:
    """Create isolated inputs and check successful tools plus unchanged outputs after rejection."""
    compiler = Path(sys.argv[1]).resolve()
    scratch = Path(__file__).resolve().parents[1] / "out" / "build"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="serializer-xsd-tools-", dir=scratch) as temporary:
        root = Path(temporary)
        source = root / "model.serializer"
        source.write_text("serializer version 1; class order { public uint32 amount; }", encoding="utf-8")
        exported = root / "exported.xsd"
        run(compiler, source, "--xml.export-xsd", str(exported))
        if not exported.is_file() or (root / "model.hpp").exists():
            raise AssertionError("Standalone XSD export must publish its requested schema only")
        output = run(compiler, source, "--xml.xsd", str(exported), "--xml.compare-xsd", str(exported), "--verbose")
        match = re.search(r"XSD bundle SHA-256 ([0-9a-f]{64})", output)
        if match is None:
            raise AssertionError("Verbose tooling did not publish its fixed bundle identity")
        path_bytes = exported.name.encode("utf-8")
        schema_bytes = exported.read_bytes()
        identity_input = b"Serializer XSD bundle 1\n"
        for value in (path_bytes, schema_bytes):
            identity_input += str(len(value)).encode("ascii") + b":" + value
        if match.group(1) != hashlib.sha256(identity_input).hexdigest():
            raise AssertionError("Bundle identity does not match its documented independent SHA-256 framing")
        run(compiler, source, "--xml.xsd", str(exported), "--xml.bundle_sha256", match.group(1), "--xml.export-xsd", str(root / "verified.xsd"))

        sentinel = root / "model.hpp"
        sentinel.write_text("preserve this output", encoding="utf-8")
        run(compiler, source, "--xml.xsd", str(exported), "--xml.bundle_sha256", "0" * 64, "--output", str(sentinel), "--cpp.format", "false", success=False)
        if sentinel.read_text(encoding="utf-8") != "preserve this output":
            raise AssertionError("Bundle pin rejection changed an existing output")

        namespace = "http://www.w3.org/2001/XMLSchema"
        element_tree.register_namespace("xs", namespace)
        current_tree = element_tree.fromstring(schema_bytes)
        current_tree.set("targetNamespace", "urn:current")
        current_tree.set("xmlns:t", "urn:current")
        for declaration in current_tree:
            if declaration.tag == f"{{{namespace}}}element":
                declaration.set("type", "t:" + declaration.attrib["type"])
        current = root / "current.xsd"
        current.write_bytes(element_tree.tostring(current_tree, encoding="utf-8"))
        result = run(compiler, source, "--xml.xsd", str(current), "--xml.compare-xsd", str(exported), "--output", str(sentinel), "--cpp.format", "false", success=False)
        if "Root expanded name changed" not in result or sentinel.read_text(encoding="utf-8") != "preserve this output":
            raise AssertionError("Incompatible root contract did not reject before publication")

        included = root / "part.xsd"
        included.write_text(f"<xs:schema xmlns:xs='{namespace}'><xs:complexType name='Order'><xs:sequence><xs:element name='amount' type='xs:unsignedInt'/></xs:sequence></xs:complexType></xs:schema>", encoding="utf-8")
        bundle = root / "root.xsd"
        bundle.write_text(f"<xs:schema xmlns:xs='{namespace}'><xs:include schemaLocation='part.xsd'/><xs:element name='order' type='Order'/></xs:schema>", encoding="utf-8")
        depfile = root / "model.d"
        run(compiler, source, "--xml.xsd", str(bundle), "--output", str(sentinel), "--cpp.format", "false", "--depfile", str(depfile), "--xml.export-xsd", str(root / "bundle-export.xsd"))
        if "part.xsd" not in depfile.read_text(encoding="utf-8"):
            raise AssertionError("Generated depfile omitted the transitive XSD include")
        before = included.read_bytes()
        run(compiler, source, "--xml.xsd", str(bundle), "--xml.export-xsd", str(included), success=False)
        if included.read_bytes() != before:
            raise AssertionError("XSD export overwrote a transitive schema input")
    print("XSD CLI tools, bundle pins, depfiles and publication checks passed")


if __name__ == "__main__":
    main()
