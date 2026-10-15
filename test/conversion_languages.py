"""Compile and execute maintained record-conversion examples with every real SDK."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)


def main():
    """Generate fresh modules, compile strictly and execute deep-copy/default/rollback assertions."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/conversion-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    selected = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    targets = {("js" if language == "javascript" else language):
               examples.generated_output(language, args.build / language,
                                         ROOT / "example" / language / "conversions")
               for language in selected}
    if "typescript" in selected and "javascript" not in selected:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "example/schemas/conversions/model.serializer", "--language", ",".join(targets),
               "--cpp.format=false", "--kotlin.package="]
    command += examples.go_generation_options(ROOT / "example/go/conversions", targets.get("go"))
    for language, destination in targets.items():
        destination.parent.mkdir(parents=True, exist_ok=True)
        command += [f"--{language}.output", destination]
    examples.run(command)
    if "typescript" in selected and "javascript" in selected:
        (args.build / "typescript/schema.mjs").write_bytes(targets["js"].read_bytes())
    runner = examples.Runner(args)
    for language in selected:
        examples.run(runner.build(language, "conversions", args.build / language))
        print(f"PASS: {language}, explicit conversions, exact types, deep value copy, defaults and generics", flush=True)


if __name__ == "__main__":
    main()
