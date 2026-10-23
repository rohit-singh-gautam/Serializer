# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Build an exact older Git revision and qualify explicit persistent-ID migration of its artifacts.

Run from a configured native compiler environment. Windows requires a Visual Studio
developer shell and PowerShell; Linux requires make. No network access or Git worktree
is needed. All generated source, archives, binaries and logs stay below --build.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import time
import zipfile


ROOT = Path(__file__).resolve().parent.parent
FIXTURE = ROOT / "qualification/managed_legacy_migration"
LEGACY_REVISION = "7b166e202668747f3a7e9a3545b3e4d75e853599"
PROFILES = {"direct32": ("uint32", False), "separate64": ("uint64", True)}
CODECS = ("binary_none", "binary_integer", "binary_string")
HISTORIES = ("disabled", "linear", "linear-labels", "tree", "tree-labels")


def sha256(path):
    """Bind provenance and exact retained artifacts to their actual bytes."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, *, cwd=ROOT, log=None):
    """Stop immediately on failed tools and retain diagnostics without evaluating shell text."""
    printable = subprocess.list2cmdline([str(item) for item in command])
    print("RUN", printable, flush=True)
    result = subprocess.run([str(item) for item in command], cwd=cwd, check=False,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            encoding="utf-8", errors="replace", timeout=1800)
    print(result.stdout, end="", flush=True)
    if log:
        with log.open("a", encoding="utf-8") as output:
            output.write(f"RUN {printable}\n{result.stdout}")
    result.check_returncode()
    return result.stdout


def cache(build):
    """Read only explicit CMake cache entries needed to preserve the selected native toolchain."""
    entries = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if line and not line.startswith(("#", "//")) and ":" in line and "=" in line:
            name, value = line.split("=", 1)
            entries[name.split(":", 1)[0]] = value
    return entries


def archive_source(directory, revision):
    """Extract a known immutable Git source snapshot without modifying refs or the active checkout."""
    marker = directory / ".serializer-legacy-revision"
    existing = directory.exists()
    if existing:
        if not marker.exists() or marker.read_text(encoding="utf-8").strip() != revision:
            raise RuntimeError("Historical source directory lacks the expected ownership/revision marker")
    archive = directory.parent / "legacy-source.zip"
    run(["git", "archive", "--format=zip", "--output", archive, revision])
    if not existing:
        directory.mkdir()
    try:
        with zipfile.ZipFile(archive) as source:
            expected = set()
            for entry in source.infolist():
                destination = (directory / entry.filename).resolve()
                if destination != directory and directory not in destination.parents:
                    raise RuntimeError("Historical archive path escaped its source directory")
                if not entry.is_dir():
                    expected.add(entry.filename)
                    if existing and (not destination.is_file() or destination.read_bytes() != source.read(entry)):
                        raise RuntimeError(f"Historical source differs from pinned Git bytes: {entry.filename}")
            if not existing:
                source.extractall(directory)
            actual = {path.relative_to(directory).as_posix() for path in directory.rglob("*") if path.is_file() and path != marker}
            # Windows IDE design-time builds can discover archived C# examples without executing them here.
            # Those ignored obj caches cannot participate in this C++-only, examples-disabled build.
            unexpected = {name for name in actual - expected if not (name.startswith("example/") and "/obj/" in name)}
            if expected - actual or unexpected:
                raise RuntimeError("Historical source contains unexpected or missing build inputs")
        marker.write_text(revision + "\n", encoding="utf-8")
        return len(expected)
    finally:
        archive.unlink(missing_ok=True)


def configure_legacy(source, build, cmake, arguments, log):
    """Configure historical source through its maintained platform wrapper, without editor packaging."""
    if os.name == "nt":
        script = build.parent / "configure-legacy.ps1"
        quote = lambda value: "'" + str(value).replace("'", "''") + "'"
        script.write_text("$ErrorActionPreference = 'Stop'\n"
                          f"Set-Location -LiteralPath {quote(source)}\n"
                          "$arguments = @(" + ", ".join(quote(value) for value in arguments) + ")\n"
                          f"& ./make.ps1 configure -Configuration Release -BuildDirectory {quote(build)} "
                          f"-CMakeCommand {quote(cmake)} -VcpkgRoot '' -CMakeArgs $arguments\n"
                          "exit $LASTEXITCODE\n", encoding="utf-8")
        try:
            run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script], log=log)
        finally:
            script.unlink(missing_ok=True)
    else:
        run(["make", "-C", source, "configure", f"BUILD_DIR={build}", "CONFIG=Release",
             f"CMAKE={cmake}", "VCPKG_ROOT=", "CMAKE_ARGS=" + " ".join(shlex.quote(str(value)) for value in arguments)], log=log)


def executable(build, target):
    """Resolve native output for the supported single- and multi-configuration build layouts."""
    suffix = ".exe" if os.name == "nt" else ""
    for candidate in (build / (target + suffix), build / "Release" / (target + suffix)):
        if candidate.exists():
            return candidate
    raise RuntimeError(f"Missing native executable: {target}")


def runtime_library(build):
    """Select the actual static runtime from the matching historical/current build."""
    names = ("serializer_runtime.lib", "libserializer_runtime.a")
    for prefix in (build, build / "Release"):
        for name in names:
            if (prefix / name).exists():
                return prefix / name
    raise RuntimeError(f"Missing static Serializer runtime in {build}")


def cmake_value(value):
    """Preserve spaces and semicolon-separated include paths without CMake string interpolation."""
    return "[==[" + str(value).replace("\\", "/") + "]==]"


def write_if_changed(path, text):
    """Preserve useful native build caches when qualification inputs have identical bytes."""
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8")


def consumer_source(directory, serializer_source, serializer_build, compiler, *, current):
    """Generate isolated CMake consumers that cannot mix historical/current headers or runtimes."""
    directory.mkdir(exist_ok=True)
    includes = ";".join(str(path) for path in (serializer_source / "include", serializer_build / "generated/include"))
    # Source headers also use local record includes, as resolved by their installed layout.
    managed_includes = ";".join(str(path) for path in (
        serializer_build / "managed/include", serializer_build / "managed/include/rohit"))
    text = f'''cmake_minimum_required(VERSION 4.0)
project(legacy_migration_qualification LANGUAGES CXX)
set(CMAKE_CXX_STANDARD {23 if current else 20})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(Serializer::runtime STATIC IMPORTED)
set_target_properties(Serializer::runtime PROPERTIES
  IMPORTED_LOCATION {cmake_value(runtime_library(serializer_build))}
  INTERFACE_INCLUDE_DIRECTORIES {cmake_value(includes)}
  INTERFACE_COMPILE_FEATURES cxx_std_{23 if current else 20} SYSTEM FALSE)
add_library(Serializer::managed INTERFACE IMPORTED)
set_target_properties(Serializer::managed PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES {cmake_value(managed_includes)}
  INTERFACE_LINK_LIBRARIES Serializer::runtime)
add_executable(Serializer::serializer IMPORTED)
set_target_properties(Serializer::serializer PROPERTIES IMPORTED_LOCATION {cmake_value(compiler)})
include({cmake_value(serializer_source / "cmake/serializer_generate.cmake")})
'''
    for profile, (id_type, separate) in PROFILES.items():
        configuration = directory / f"{profile}.ini"
        write_if_changed(configuration, "[cpp]\nformat = false\n[managed]\n"
                         f"id_type = {id_type}\nseparate_values = {str(separate).lower()}\n"
                         + ("id_encoding = compact_varint\n" if current else ""))
        generated = "${CMAKE_CURRENT_BINARY_DIR}/generated/" + profile
        if current:
            target = "migrate_" + profile
            text += f'''serializer_add_migration_tool(TARGET {target}
  SCHEMA {cmake_value(FIXTURE / "model.serializer")} ROOT legacy_migration::document
  CONFIG {cmake_value(configuration)} OUTPUT_DIRECTORY "{generated}")
add_executable(verify_{profile} {cmake_value(FIXTURE / "current_verifier.cpp")})
add_dependencies(verify_{profile} {target})
target_include_directories(verify_{profile} PRIVATE "{generated}")
target_link_libraries(verify_{profile} PRIVATE Serializer::managed)
'''
            targets = (target, "verify_" + profile)
        else:
            target = "produce_" + profile
            text += f'''add_executable({target} {cmake_value(FIXTURE / "legacy_producer.cpp")})
serializer_generate(TARGET {target} SCHEMAS {cmake_value(FIXTURE / "model.serializer")}
  CONFIG {cmake_value(configuration)} OUTPUT_DIRECTORY "{generated}")
target_link_libraries({target} PRIVATE Serializer::managed)
'''
            targets = (target,)
        for target in targets:
            text += f'''if(MSVC)
  target_compile_options({target} PRIVATE /W4 /WX /EHsc)
else()
  target_compile_options({target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
'''
    write_if_changed(directory / "CMakeLists.txt", text)


def migrate(compiler, directory, source, target, kind, log, source_codec=None, target_codec=None):
    """Invoke the actual schema-linked CLI in explicit directions without replacing an existing file."""
    original = source.read_bytes()
    target.unlink(missing_ok=True)
    command = [compiler, "--kind", kind, "--from", "fixed", "--to", "compact_varint",
               "--input", source, "--output", target]
    if source_codec:
        command.extend(["--input-format", source_codec, "--output-format", target_codec])
    run(command, log=log)
    restored = directory / (target.stem + ".restored")
    restored.unlink(missing_ok=True)
    reverse = [compiler, "--kind", kind, "--from", "compact_varint", "--to", "fixed",
               "--input", target, "--output", restored]
    if source_codec:
        reverse.extend(["--input-format", target_codec, "--output-format", source_codec])
    run(reverse, log=log)
    if source.read_bytes() != original:
        raise AssertionError(f"Migration modified its old input artifact: {source.name}")
    if restored.read_bytes() != original:
        raise AssertionError(f"Old fixed bytes changed after complete roundtrip: {source.name}")
    return 2


def main():
    """Record genuine older compiler artifacts and exact identity/history-preserving migration evidence."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True, help="Current built Serializer compiler")
    parser.add_argument("--current-build", type=Path, required=True, help="Matching current native CMake build")
    parser.add_argument("--build", type=Path, required=True, help="Dedicated ignored qualification directory")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.current_build = args.current_build.resolve()
    directory = args.build.resolve()
    if directory == ROOT or ROOT in directory.parents and "out" not in directory.relative_to(ROOT).parts:
        parser.error("In-repository output must remain below ignored out")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    directory.mkdir(parents=True, exist_ok=True)
    log = directory / "qualification.log"
    log.write_text("", encoding="utf-8")
    started = time.perf_counter()
    revision = run(["git", "rev-parse", LEGACY_REVISION]).strip()
    old_source, old_build = directory / "legacy-source", directory / "legacy-build"
    legacy_source_files = archive_source(old_source, revision)
    selected = cache(args.current_build)
    native_arguments = ["-G", selected["CMAKE_GENERATOR"]]
    for name in ("CMAKE_MAKE_PROGRAM", "CMAKE_CXX_COMPILER"):
        if selected.get(name):
            native_arguments.append(f"-D{name}={selected[name]}")
    if selected.get("CMAKE_GENERATOR_PLATFORM"):
        native_arguments.extend(["-A", selected["CMAKE_GENERATOR_PLATFORM"]])
    configure_legacy(old_source, old_build, args.cmake, native_arguments + [
        "-DSERIALIZER_BUILD_TESTS=OFF", "-DSERIALIZER_BUILD_MANAGED=ON", "-DSERIALIZER_INSTALL=OFF",
        "-DSERIALIZER_ENABLE_SIMD=OFF", "-DSERIALIZER_WITH_XML=OFF", "-DSERIALIZER_WITH_ZSTD=OFF",
        "-DSERIALIZER_WITH_LZ4=OFF", "-DSERIALIZER_WITH_ZLIB=OFF"], log)
    run([args.cmake, "--build", old_build, "--config", "Release", "--parallel", args.jobs,
         "--target", "serializer", "serializer_managed_records"], log=log)
    old_compiler = executable(old_build, "serializer")
    old_version = run([old_compiler, "--version"], log=log)
    if "Serializer compiler 2.6.0\n" not in old_version or "1.19.0" not in old_version:
        raise AssertionError("Historical compiler version does not match the pinned source")
    current_version = run([args.compiler, "--version"], log=log)
    for name, source, build, compiler, current in (
        ("old-consumer", old_source, old_build, old_compiler, False),
        ("current-consumer", ROOT, args.current_build, args.compiler, True),
    ):
        consumer = directory / name
        consumer_source(consumer, source, build, compiler, current=current)
        run([args.cmake, "-S", consumer, "-B", directory / (name + "-build"),
             "-DCMAKE_BUILD_TYPE=Release", *native_arguments], log=log)
        run([args.cmake, "--build", directory / (name + "-build"), "--config", "Release",
             "--parallel", args.jobs], log=log)
    operations = 0
    artifacts = {}
    for profile in PROFILES:
        artifact_directory = directory / profile
        artifact_directory.mkdir(exist_ok=True)
        run([executable(directory / "old-consumer-build", "produce_" + profile), artifact_directory], log=log)
        cli = executable(directory / "current-consumer-build", "migrate_" + profile)
        for history in HISTORIES:
            source = artifact_directory / f"managed-{history}.fixed"
            operations += migrate(cli, artifact_directory, source, source.with_suffix(".compact"), "managed", log)
        for variant in ("normal", "boundary"):
            for source_codec in CODECS:
                source = artifact_directory / f"raw-{variant}-{source_codec}.fixed"
                for target_codec in CODECS:
                    target = artifact_directory / f"raw-{variant}-{source_codec}-{target_codec}.compact"
                    operations += migrate(cli, artifact_directory, source, target, "raw", log, source_codec, target_codec)
        run([executable(directory / "current-consumer-build", "verify_" + profile), artifact_directory], log=log)
        artifacts[profile] = {path.name: sha256(path) for path in sorted(artifact_directory.iterdir()) if path.is_file()}
    provenance = {
        "legacy_revision": revision,
        "legacy_source_files_verified_against_git": legacy_source_files,
        "legacy_compiler_version": old_version.strip(),
        "legacy_compiler_sha256": sha256(old_compiler),
        "legacy_runtime_sha256": sha256(runtime_library(old_build)),
        "current_git_head": run(["git", "rev-parse", "HEAD"]).strip(),
        "current_compiler_version": current_version.strip(),
        "current_compiler_sha256": sha256(args.compiler),
        "current_runtime_sha256": sha256(runtime_library(args.current_build)),
        "fixture_sha256": {path.name: sha256(path) for path in sorted(FIXTURE.iterdir()) if path.is_file()},
        "profiles": {key: {"id_type": value[0], "separate_values": value[1]} for key, value in PROFILES.items()},
        "migration_cli_operations": operations,
        "artifacts_sha256": artifacts,
        "elapsed_seconds": time.perf_counter() - started,
        "platform": sys.platform,
    }
    (directory / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: {operations} actual CLI migrations, 36 old raw codec/boundary cases, ten old save/history cases; exact reverse bytes and current native identities/history/nonreuse verified", flush=True)


if __name__ == "__main__":
    main()
