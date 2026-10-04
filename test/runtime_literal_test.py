"""Keep embedded runtime literals within the Visual Studio 2022 C2026 limit."""

from pathlib import Path
import re
import unittest


# MSVC checks each token before concatenating adjacent string literals.
MAX_LITERAL_BYTES = 16380
RAW_LITERAL = re.compile(rb'R"([^ ()\\\t\r\n]{0,16})\((.*?)\)\1"', re.DOTALL)
SOURCE_DIRECTORY = Path(__file__).resolve().parents[1] / "src"


class RuntimeLiteralTest(unittest.TestCase):
    """Check every embedded runtime, including newly added output languages."""

    def test_runtime_literals(self):
        """Reject oversized tokens and unexpected text outside adjacent literals."""
        paths = sorted(SOURCE_DIRECTORY.glob("*_runtime.inc"))
        self.assertTrue(paths, "No embedded runtimes found")
        for path in paths:
            with self.subTest(runtime=path.name):
                # C++ normalizes source line endings before reading raw strings.
                source = path.read_bytes().replace(b"\r\n", b"\n")
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
