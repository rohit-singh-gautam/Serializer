"""Compile generated Go XML for nested/multiple base composition and qualified field order."""
import argparse
from pathlib import Path
import subprocess


MODEL = '''serializer version 1.20.0;
namespace inherited {
  class first { public uint32 a; }
  class second { public string b; }
  class middle : public first { public bool c; }
  class record : public middle, public second { public int16 d; }
}
'''

GO = r'''package main
import ("fmt"; "reflect"; "strings")
// Require base-first XML ordering while retaining native nested base ownership.
func main() {
  document := "<record><a>4294967295</a><c>true</c><b>A&amp;B</b><d>-32768</d></record>"
  value, err := DecodeInheritedRecordXML([]byte(document)); if err != nil { panic(err) }
  if value.Base0.Base0.A != 4294967295 || !value.Base0.C || value.Base1.B != "A&B" || value.D != -32768 { panic("Flattened base values") }
  data, err := value.EncodeXML(); if err != nil { panic(err) }
  if string(data) != document { panic("Base declaration order changed") }
  copy, err := DecodeInheritedRecordXML(data); if err != nil { panic(err) }
  first, err := value.Encode(BINARY_NONE); if err != nil { panic(err) }
  second, err := copy.Encode(BINARY_NONE); if err != nil || !reflect.DeepEqual(first, second) { panic("Native base composition changed") }
  for _, invalid := range []string{
    strings.Replace(document, "<a>4294967295</a><c>true</c>", "<c>true</c><a>4294967295</a>", 1),
    strings.Replace(document, "<b>A&amp;B</b>", "", 1),
    strings.Replace(document, "</record>", "<a>1</a></record>", 1),
  } { if candidate, err := DecodeInheritedRecordXML([]byte(invalid)); candidate != nil || err == nil { panic("Invalid inherited candidate published") } }
  value.Base0.Base0 = nil
  if partial, err := value.EncodeXML(); partial != nil || err == nil { panic("Nil nested base published") }
  value.Base0 = nil
  if partial, err := value.EncodeXML(); partial != nil || err == nil { panic("Nil parent base published") }
  fmt.Println("Go generated inherited XML ordering, composition and transactional rejection pass")
}
'''


def main():
    """Generate through the real compiler and execute the installed Go SDK."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.build = args.build.resolve()
    root = Path(__file__).resolve().parent.parent
    if args.build == root or root in args.build.parents and 'out' not in args.build.relative_to(root).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    model = args.build / 'model.serializer'
    model.write_text(MODEL, encoding='utf-8')
    subprocess.run([str(args.compiler.resolve()), '--input', str(model), '--language', 'go',
                    '--go.output', str(args.build / 'schema.go'), '--go.package', 'main',
                    '--xml.enabled', 'true', '--protocols', 'binary_none'], check=True)
    (args.build / 'main.go').write_text(GO, encoding='utf-8')
    (args.build / 'go.mod').write_text('module serializer-go-xml-inherited\n\ngo 1.23\n', encoding='utf-8')
    subprocess.run(['go', 'run', '.'], cwd=args.build, check=True)
    # The binder must diagnose native name collisions before emitting a flattened mapping.
    model.write_text('serializer version 1.20.0; class first { public uint32 same; } '
                     'class second { public string same; } '
                     'class record : public first, public second {}', encoding='utf-8')
    result = subprocess.run([str(args.compiler.resolve()), '--input', str(model), '--language', 'go',
                             '--go.output', str(args.build / 'collision.go'), '--xml.enabled', 'true'],
                            capture_output=True, text=True)
    if result.returncode == 0 or 'ambiguous' not in result.stdout + result.stderr:
        raise AssertionError('Expected an inherited XML field collision diagnostic')
    if (args.build / 'collision.go').exists():
        raise AssertionError('Invalid inheritance published generated output')


if __name__ == '__main__':
    main()
