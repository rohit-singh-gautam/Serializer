"""Compile and execute derived record-size annotations against every supported SDK."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)
FILES = {"cpp": "main.cpp", "java": "Main.java", "javascript": "main.mjs", "typescript": "main.mts",
         "go": "main.go", "csharp": "Program.cs", "rust": "main.rs", "python": "main.py",
         "swift": "main.swift", "kotlin": "Main.kt", "c": "main.c"}
OWNERS = ("sized_payload", "sized_envelope", "split_sized", "sized_magic", "wire_sized", "payload_sized")


def consumer(language, retain):
    """Select independently maintained consumers with the same schema and validation policy."""
    if language in ("cpp", "c", "rust", "python"):
        from auto_size_consumers_native import consumer as selected
    elif language in ("java", "swift", "kotlin"):
        from auto_size_consumers_mobile import consumer as selected
    else:
        from auto_size_consumers_portable import consumer as selected
    return selected(language, retain)


def main():
    """Build separate verify/retain modules and require their structural checks to agree."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/auto-size-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    parser.add_argument("--policy", choices=("all", "verify", "retain"), default="all")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    runner = examples.Runner(args)
    for retain in (False, True):
        policy = "retain" if retain else "verify"
        if args.policy != "all" and args.policy != policy:
            continue
        build = args.build / policy
        targets = {("js" if language == "javascript" else language):
                   build / language / examples.OUTPUTS[language] for language in languages}
        if "typescript" in languages and "javascript" not in languages:
            targets["js"] = build / "typescript/schema.mjs"
        configuration = build / "auto_size.ini"
        configuration.parent.mkdir(parents=True, exist_ok=True)
        text = "[cpp]\nformat = false\n"
        for language in targets:
            for owner in OWNERS:
                text += f"\n[{language}.class.{owner}]\nauto_size_validation = {policy}\n"
        configuration.write_text(text, encoding="utf-8")
        command = [args.compiler, "--input", ROOT / "example/schemas/derived_sizes/model.serializer",
                   "--config", configuration, "--language", ",".join(targets),
                   "--go.package=main", "--kotlin.package="]
        for language, destination in targets.items():
            destination.parent.mkdir(parents=True, exist_ok=True)
            command += [f"--{language}.output", destination]
        examples.run(command)
        if "typescript" in languages and "javascript" in languages:
            (build / "typescript/schema.mjs").write_bytes(targets["js"].read_bytes())
        for language in languages:
            source = build / "consumers" / language
            source.mkdir(parents=True, exist_ok=True)
            (source / FILES[language]).write_text(consumer(language, retain), encoding="utf-8")
            examples.run(runner.build(language, "derived_sizes", build / language, source))
            print(f"PASS: {language}, {policy}, computed total/payload record sizes, wire-only metadata, "
                  "nested scopes, source immutability and bounded structural decoding", flush=True)


if __name__ == "__main__":
    main()
