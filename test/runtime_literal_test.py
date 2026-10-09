"""Keep embedded runtime literals within the Visual Studio 2022 C2026 limit."""

from pathlib import Path
import re
import unittest


# MSVC checks each token before concatenating adjacent string literals.
MAX_LITERAL_BYTES = 16380
RAW_LITERAL = re.compile(rb'R"([^ ()\\\t\r\n]{0,16})\((.*?)\)\1"', re.DOTALL)
RUNTIME_LICENSE_PROLOGUE = (
    b"// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)\n"
    b"// SPDX-License-Identifier: 0BSD\n"
    b"// Serializer runtime template source; emitted support may also be used under 0BSD.\n"
)
SOURCE_DIRECTORY = Path(__file__).resolve().parents[1] / "src"


class RuntimeLiteralTest(unittest.TestCase):
    """Check every embedded runtime, including newly added output languages."""

    def test_runtime_literals(self):
        """Allow the source license prologue, then require bounded adjacent raw literals only."""
        paths = sorted(SOURCE_DIRECTORY.glob("*_runtime.inc"))
        self.assertTrue(paths, "No embedded runtimes found")
        for path in paths:
            with self.subTest(runtime=path.name):
                # C++ normalizes source line endings before reading raw strings.
                source = path.read_bytes().replace(b"\r\n", b"\n")
                self.assertTrue(
                    source.startswith(RUNTIME_LICENSE_PROLOGUE),
                    "Expected the 0BSD source license outside the emitted raw strings",
                )
                source = source[len(RUNTIME_LICENSE_PROLOGUE):]
                literals = list(RAW_LITERAL.finditer(source))
                self.assertTrue(literals, "No raw string literals found")
                self.assertEqual(RAW_LITERAL.sub(b"", source).strip(), b"")
                for index, literal in enumerate(literals):
                    self.assertLessEqual(
                        len(literal[2]), MAX_LITERAL_BYTES,
                        f"{path.name} literal {index + 1}: split into adjacent raw strings",
                    )


if __name__ == "__main__":
    unittest.main()
