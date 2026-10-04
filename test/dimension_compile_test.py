"""Check native template storage constraints with a configured C++ compiler environment."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    """Compile application-only dimensions and require each invalid extent to fail."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--generated', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    msvc = Path(args.compiler).name.lower() in ('cl', 'cl.exe')
    cases = [('4', True), ('0', False), ('-1', False), ('65537,1', False),
             ('4294967296ULL,4294967296ULL', False)]
    with tempfile.TemporaryDirectory(prefix='serializer-dimension-compile-') as temporary:
        directory = Path(temporary)
        for arguments, valid in cases:
            source = directory / 'consumer.cpp'
            source.write_text('#include <dimensions.hpp>\n'
                              f'dimensions::matrix<{arguments}> value;\n')
            output = directory / 'consumer.obj'
            command = ([args.compiler, '/nologo', '/std:c++20', '/EHsc', '/c',
                        '/I' + str(root / 'include'), '/I' + str(args.generated.resolve()),
                        '/Fo' + str(output), str(source)] if msvc else
                       [args.compiler, '-std=c++20', '-c', '-I' + str(root / 'include'),
                        '-I' + str(args.generated.resolve()), '-o', str(output), str(source)])
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if (result.returncode == 0) != valid:
                raise RuntimeError(f'matrix<{arguments}>: unexpected compiler outcome\n{result.stdout}')
            print(f'matrix<{arguments}>: {"compiled" if valid else "rejected"}')


if __name__ == '__main__':
    main()
