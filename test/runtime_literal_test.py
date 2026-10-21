"""Keep embedded runtime literals within the Visual Studio 2022 C2026 limit."""

from pathlib import Path
import re
import unittest


# MSVC checks each token before concatenating adjacent string literals.
MAX_LITERAL_BYTES = 16380
RAW_LITERAL = re.compile(rb'R"([^ ()\\\t\r\n]{0,16})\((.*?)\)\1"', re.DOTALL)
RUNTIME_INCLUDE = re.compile(rb'^#include "([a-z][a-z0-9_]*_runtime\.inc)"[ \t]*$', re.MULTILINE)
RUNTIME_LICENSE_PROLOGUE = (
    b"// SPDX-License-Identifier: 0BSD\n"
    b"// Serializer runtime template source; emitted support may also be used under 0BSD.\n"
)
RUNTIME_COPYRIGHT = re.compile(
    rb"// Copyright \(C\) (?:2024, )?2026 Rohit Jairaj Singh \(rohit@singh.org.in\)\n"
)
SOURCE_DIRECTORY = Path(__file__).resolve().parents[1] / "src"


class RuntimeLiteralTest(unittest.TestCase):
    """Check every embedded runtime, including newly added output languages."""

    def test_runtime_literals(self):
        """Require bounded raw literals and sibling runtime includes checked by the same inventory."""
        paths = sorted(SOURCE_DIRECTORY.glob("*_runtime.inc"))
        runtime_names = {path.name for path in paths}
        self.assertTrue(paths, "No embedded runtimes found")
        for path in paths:
            with self.subTest(runtime=path.name):
                # C++ normalizes source line endings before reading raw strings.
                source = path.read_bytes().replace(b"\r\n", b"\n")
                copyright_line = RUNTIME_COPYRIGHT.match(source)
                self.assertIsNotNone(copyright_line, "Expected the runtime's source copyright")
                source = source[copyright_line.end():]
                self.assertTrue(
                    source.startswith(RUNTIME_LICENSE_PROLOGUE),
                    "Expected the 0BSD source license outside the emitted raw strings",
                )
                source = source[len(RUNTIME_LICENSE_PROLOGUE):]
                literals = list(RAW_LITERAL.finditer(source))
                self.assertTrue(literals, "No raw string literals found")
                # Includes concatenate independently checked tokens; never allow paths,
                # arbitrary headers or other C++ outside the bounded literal inventory.
                remaining = RAW_LITERAL.sub(b"", source)
                for include in RUNTIME_INCLUDE.finditer(remaining):
                    self.assertIn(include[1].decode("ascii"), runtime_names)
                self.assertEqual(RUNTIME_INCLUDE.sub(b"", remaining).strip(), b"")
                for index, literal in enumerate(literals):
                    self.assertLessEqual(
                        len(literal[2]), MAX_LITERAL_BYTES,
                        f"{path.name} literal {index + 1}: split into adjacent raw strings",
                    )


if __name__ == "__main__":
    unittest.main()
