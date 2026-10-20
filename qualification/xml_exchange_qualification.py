# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Retain actual eleven-language configured/XSD XML exchange evidence without skipping absent SDKs."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
PROFILES = ("configured", "xsd")


def digest(path):
    """Hash an existing source/tool/output while preserving its bytes and ownership."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_runner():
    """Reuse the maintained XML fixture, SDK builds, independent oracle and full pair loop."""
    spec = importlib.util.spec_from_file_location("serializer_xml_exchange", ROOT / "example/xml/run.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def seed_cache(source, destination, children):
    """Copy cached locked dependencies into private output; never modify the seed installation."""
    if source is None:
        return
    source = source.resolve()
    if not source.is_dir():
        raise FileNotFoundError(source)
    for child in children:
        existing = source / child
        if existing.exists():
            shutil.copytree(existing, destination / child, dirs_exist_ok=True)


def prepare_environment(args):
    """Keep package restoration offline and isolate mutable SDK caches beneath this qualification output."""
    caches = args.build / "sdk-caches"
    seed_cache(args.npm_cache_seed, caches / "npm", ("_cacache",))
    seed_cache(args.cargo_home_seed, caches / "cargo", ("registry/cache", "registry/index"))
    temporary = args.build / "temporary"
    temporary.mkdir(parents=True, exist_ok=True)
    os.environ.update(npm_config_cache=str(caches / "npm"), npm_config_offline="true",
                      CARGO_HOME=str(caches / "cargo"), CARGO_NET_OFFLINE="true",
                      GOTOOLCHAIN="local", GOWORK="off", GOPROXY="off", GOSUMDB="off",
                      GOCACHE=str(caches / "go"), DOTNET_CLI_HOME=str(caches / "dotnet"),
                      DOTNET_SKIP_FIRST_TIME_EXPERIENCE="1", DOTNET_CLI_TELEMETRY_OPTOUT="1",
                      PYTHONDONTWRITEBYTECODE="1", TMP=str(temporary), TEMP=str(temporary))
    if args.kotlinc:
        os.environ["SERIALIZER_KOTLINC"] = str(args.kotlinc.resolve())
    if args.java_home:
        home = args.java_home.resolve()
        suffix = ".exe" if os.name == "nt" else ""
        os.environ.update(JAVA_HOME=str(home), SERIALIZER_JAVA=str(home / "bin" / ("java" + suffix)),
                          SERIALIZER_JAVAC=str(home / "bin" / ("javac" + suffix)))


class Trace:
    """Capture every real SDK/generator/consumer process and preserve the common runner's failure semantics."""
    def __init__(self, output, creation_flags, native_compiler=None):
        """Own only per-profile diagnostics and command records, with no SDK substitution."""
        self.output = output
        self.creation_flags = creation_flags
        self.commands = []
        self.native_compiler = native_compiler
        (output / "commands").mkdir(parents=True, exist_ok=True)

    def run(self, command, *, cwd=None, capture=False):
        """Execute actual argv, record bounded-duration diagnostics, and propagate every failed command."""
        return self.execute(command, cwd=cwd, capture=capture)

    def execute(self, command, *, cwd=None, capture=False, rejection=False):
        """Retain expected negative consumer exits separately from successful build/run commands."""
        command = [str(item) for item in command]
        if self.native_compiler and Path(command[0]).resolve() == self.native_compiler and "--input" in command:
            command += ["--xml.enabled", "false"]
        if self.native_compiler and "/std:c11" in command:
            # Match the maintained CMake C example's standard-CRT declaration policy.
            command.append("/D_CRT_SECURE_NO_WARNINGS")
        if self.native_compiler and Path(command[0]).stem.lower() in ("java", "javac"):
            # A native-only model must also run without the JDK's optional java.xml module.
            command[1:1] = ["--limit-modules", "java.base"]
        index = len(self.commands) + 1
        log = self.output / "commands" / (f"{index:04d}.log")
        beginning = time.monotonic()
        result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=600, creationflags=self.creation_flags)
        log.write_bytes(result.stdout)
        record = {"argv": command, "cwd": str(cwd) if cwd else None, "returncode": result.returncode,
                  "elapsed_seconds": round(time.monotonic() - beginning, 3),
                  "log": str(log.relative_to(self.output)), "log_sha256": digest(log),
                  "expected_nonzero": rejection}
        if len(command) == 2 and command[1] in ("version", "--version", "-version"):
            record["sdk_version"] = result.stdout.decode("utf-8", errors="replace").strip()
        self.commands.append(record)
        if bool(result.returncode) != rejection:
            raise RuntimeError(f"Command {index} failed ({result.returncode}): {command}\n"
                               + result.stdout.decode("utf-8", errors="replace")[-16000:])
        return subprocess.CompletedProcess(command, result.returncode,
                                           result.stdout.decode("utf-8") if capture else None)


def native_disabled(module, args, tools, manifest):
    """Build/run all four native formats with XML generation disabled and no parser link/import dependencies."""
    directory = args.build / "native-disabled"
    trace = Trace(directory, module.sdk.PROCESS_CREATION_FLAGS, tools["compiler"])
    original_run, original_rejection = module.sdk.run, module.sdk.require_rejection
    original_tsc = os.environ.get("SERIALIZER_TSC")
    tsc = args.build / "configured/typescript/node_modules/.bin" / ("tsc.cmd" if os.name == "nt" else "tsc")
    os.environ["SERIALIZER_TSC"] = str(tsc)
    module.sdk.run = trace.run

    def rejected(command, output):
        """Require the maintained future-version consumer to fail without publishing a candidate file."""
        output.unlink(missing_ok=True)
        trace.execute(command, rejection=True)
        if output.exists():
            raise AssertionError("Rejected native payload published output: " + str(output))

    module.sdk.require_rejection = rejected
    languages = [entry["id"] for entry in manifest["languages"]]
    options = argparse.Namespace(compiler=tools["compiler"], build=directory, wsl_languages="",
                                cpp_library=tools["cpp_library"], executable=None, sanitize=False,
                                big_endian=False, version_policy_as_of=None)
    try:
        module.sdk.Runner(options).example("basic", languages)
        blocked = {"cpp": ("<rohit/xml.hpp>",), "c": ("<libxml/",),
                   "java": ("javax.xml.",), "kotlin": ("javax.xml.", "org.xml.sax"),
                   "python": ("import xml.", "from xml."), "go": ('"encoding/xml"',),
                   "javascript": ("@xmldom/xmldom", "saxes", "xmlchars"),
                   "typescript": ("@xmldom/xmldom", "saxes", "xmlchars"),
                   "rust": ("roxmltree::", "xmlparser::"), "swift": ("import FoundationXML",),
                   "csharp": ("System.Xml",)}
        generated = {}
        basic = directory / "basic"
        for language in languages:
            name = module.sdk.generated_output(language, basic / language, ROOT / "example" / language / "basic")
            text = name.read_text(encoding="utf-8")
            if any(token in text for token in blocked[language]):
                raise AssertionError("Disabled XML retained parser imports: " + language)
            generated[language] = digest(name)
        linked = {}
        for language in ("c", "cpp"):
            executable = basic / language / ("main.exe" if os.name == "nt" else "main")
            inspector = "dumpbin" if os.name == "nt" else "ldd"
            command = [shutil.which(inspector)]
            if not command[0]:
                raise RuntimeError("Native dependency inspector unavailable: " + inspector)
            if os.name == "nt":
                command.append("/dependents")
            result = trace.run([*command, executable], capture=True)
            if "libxml" in result.stdout.lower() or "xml_provider_bridge" in result.stdout.lower():
                raise AssertionError("Native-only executable imports an XML provider: " + language)
            linked[language] = {"executable_sha256": digest(executable), "xml_provider_imports": False}
        return {"status": "passed", "languages": languages, "native_protocols_per_language": 4,
                "native_protocol_roundtrips": len(languages) * 4, "future_version_rejections": len(languages),
                "xml_disabled": True, "generated_sha256": generated, "native_dependency_checks": linked}
    finally:
        module.sdk.run, module.sdk.require_rejection = original_run, original_rejection
        if original_tsc is None:
            os.environ.pop("SERIALIZER_TSC", None)
        else:
            os.environ["SERIALIZER_TSC"] = original_tsc
        (directory / "commands.json").write_text(json.dumps(trace.commands, indent=2) + "\n", encoding="utf-8")


def check_profile(module, directory, manifest, header):
    """Require all132 independently checked documents and one actual generated source for every backend."""
    languages = [entry["id"] for entry in manifest["languages"]]
    outputs = {}
    for producer in languages:
        for name in [producer + ".xml", *[producer + "-to-" + consumer + ".xml" for consumer in languages]]:
            path = directory / name
            module.fixtures.check_document(path, manifest["expected"])
            outputs[name] = digest(path)
    generated = {}
    for language in languages:
        name = "internal/model/model_gen.go" if language == "go" else module.sdk.OUTPUTS[language]
        generated[language] = digest(directory / language / name)
    if header:
        if (directory / "cpp/message.hpp").read_bytes() != header.read_bytes():
            raise AssertionError("Fresh C++ output differs from the header used by the copied native helper")
    return {"languages": languages, "producers": len(languages), "consumer_pairs": len(languages) ** 2,
            "independent_xml_documents": len(outputs), "generated_sha256": generated,
            "document_sha256": outputs, "cpp_generated_header_matches": bool(header)}


def main():
    """Qualify both maintained XML mapping profiles and retain exact selected SDK/tool/source provenance."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--cpp-configured", required=True, type=Path)
    parser.add_argument("--cpp-xsd", required=True, type=Path)
    parser.add_argument("--cpp-library", required=True, type=Path)
    parser.add_argument("--cpp-configured-header", type=Path)
    parser.add_argument("--cpp-xsd-header", type=Path)
    parser.add_argument("--libxml2-prefix", type=Path)
    parser.add_argument("--kotlinc", type=Path)
    parser.add_argument("--java-home", type=Path)
    parser.add_argument("--npm-cache-seed", type=Path)
    parser.add_argument("--cargo-home-seed", type=Path)
    args = parser.parse_args()
    args.build = args.build.resolve()
    if args.build == ROOT or ROOT in args.build.parents and "out" not in args.build.relative_to(ROOT).parts:
        parser.error("In-repository output must stay beneath ignored out/")
    args.build.mkdir(parents=True, exist_ok=True)
    if (args.build / "verification.json").exists():
        parser.error("Choose a fresh output directory; retained qualification evidence is immutable")
    tools = {name: getattr(args, name).resolve() for name in ("compiler", "cpp_configured", "cpp_xsd", "cpp_library")}
    before = {name: digest(path) for name, path in tools.items()}
    prepare_environment(args)
    module = load_runner()
    manifest_path = ROOT / "test/resources/xml_exchange_manifest.json"
    manifest = module.fixtures.load_manifest(manifest_path, ROOT)
    source_paths = {"example/xml/run.py", "example/xml/fixture_manifest.py", "example/run.py",
                    "example/xml/model.serializer", "example/xml/codegen.ini", "example/xml/xsd.ini",
                    "example/xml/order.xsd", "example/xml/order.xml", "example/xml/c/CMakeLists.txt",
                    "example/typescript/node.d.mts", "test/resources/xml_exchange_manifest.json"}
    source_paths.update(entry["source"] for entry in manifest["languages"])
    for language in ("rust", "javascript", "typescript"):
        source_paths.update(path.relative_to(ROOT).as_posix() for path in (ROOT / "example/xml" / language).iterdir()
                            if path.is_file() and path.name != "README.md")
    source_paths.update(("example/schemas/basic/model.serializer", "example/schemas/basic/fixture.json"))
    native_suffixes = {".cpp", ".c", ".java", ".mjs", ".mts", ".go", ".cs", ".rs", ".py", ".swift", ".kt", ".mod", ".csproj"}
    for entry in manifest["languages"]:
        source_paths.update(path.relative_to(ROOT).as_posix() for path in (ROOT / "example" / entry["id"] / "basic").iterdir()
                            if path.is_file() and path.suffix in native_suffixes)
    source_paths.update(path.relative_to(ROOT).as_posix() for path in (ROOT / "include").rglob("*.hpp"))
    report = {"status": "failed", "manifest_sha256": digest(manifest_path),
              "tools": {name: {"path": str(path), "sha256": before[name]} for name, path in tools.items()},
              "sources": {name: digest(ROOT / name) for name in sorted(source_paths)}, "profiles": {},
              "python": sys.version, "host": sys.platform,
              "compiler_version": subprocess.check_output([str(tools["compiler"]), "--version"], text=True, encoding="utf-8").strip(),
              "scope": "Actual configured/XSD eleven-language XML exchange, plus XML-disabled all-four-native-protocol builds with parser import/link negative checks. Existing example negatives do not establish the uniform Q02 resource/interruption corpus; no performance or process-memory gate is inferred."}
    beginning = time.monotonic()
    original_run = module.sdk.run
    try:
        for profile in PROFILES:
            print("PROFILE " + profile + ": actual11 SDK builds and121 exchanges", flush=True)
            directory = args.build / profile
            trace = Trace(directory, module.sdk.PROCESS_CREATION_FLAGS)
            module.sdk.run = trace.run
            arguments = ["xml-example", "--compiler", str(tools["compiler"]), "--build", str(directory),
                         "--cpp-executable", str(tools["cpp_" + profile]),
                         "--config", str(ROOT / "example/xml" / ("codegen.ini" if profile == "configured" else "xsd.ini"))]
            if args.libxml2_prefix:
                arguments += ["--libxml2-prefix", str(args.libxml2_prefix.resolve())]
            sys.argv = arguments
            try:
                module.main()
                result = check_profile(module, directory, manifest, getattr(args, "cpp_" + profile + "_header"))
                result["status"] = "passed"
                report["profiles"][profile] = result
            finally:
                (directory / "commands.json").write_text(json.dumps(trace.commands, indent=2) + "\n", encoding="utf-8")
            print("PASS " + profile + ":121 pairs and132 independent XML oracles", flush=True)
        print("PROFILE native-disabled: actual11 SDK builds and4 native formats", flush=True)
        report["native_disabled"] = native_disabled(module, args, tools, manifest)
        assert before == {name: digest(path) for name, path in tools.items()}, "Selected immutable tool changed"
        report["status"] = "passed"
    except BaseException as failure:
        report["failure"] = str(failure)
        raise
    finally:
        module.sdk.run = original_run
        report["elapsed_seconds"] = round(time.monotonic() - beginning, 3)
        report["runner_sha256"] = digest(Path(__file__))
        (args.build / "verification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "profiles": len(report["profiles"]),
                      "consumer_pairs": sum(row["consumer_pairs"] for row in report["profiles"].values()),
                      "independent_xml_documents": sum(row["independent_xml_documents"] for row in report["profiles"].values()),
                      "native_protocol_roundtrips": report["native_disabled"]["native_protocol_roundtrips"],
                      "elapsed_seconds": report["elapsed_seconds"]}, indent=2))


if __name__ == "__main__":
    main()
