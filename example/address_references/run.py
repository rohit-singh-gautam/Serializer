"""Generate, build and run the maintained field-reference and translator examples."""
import argparse
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SPEC = importlib.util.spec_from_file_location("serializer_examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)


def main():
    """Use the shared SDK builder with isolated generated output and sequential consumers."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/address-reference-examples")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    if not languages or len(set(languages)) != len(languages) or any(language not in examples.LANGUAGES for language in languages):
        parser.error("Select unique supported language names, or all")
    targets = {("js" if language == "javascript" else language):
               examples.generated_output(language, args.build / language, HERE / language)
               for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", HERE / "model.serializer", "--language", ",".join(targets),
               "--cpp.format=false", "--kotlin.package="]
    command += examples.go_generation_options(HERE / "go", targets.get("go"))
    for language, destination in targets.items():
        destination.parent.mkdir(parents=True, exist_ok=True)
        command += [f"--{language}.output", destination]
    examples.run(command)
    if "typescript" in languages and "javascript" in languages:
        (args.build / "typescript/schema.mjs").write_bytes(targets["js"].read_bytes())
    runner = examples.Runner(args)
    for language in languages:
        examples.run(runner.build(language, "address_references", args.build / language, HERE / language))
        print(f"PASS: {language}, live field references, explicit resolution and scoped JSON projection", flush=True)


if __name__ == "__main__":
    main()
