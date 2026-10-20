# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Prepare the exact locked parser and TypeScript dependencies for package SDK tests."""
import hashlib
import json
import os
from pathlib import Path
import shutil


def prepare_node_dependencies(build, supplied, run):
    """Reuse exact dependencies or restore their lock, preferring cached packages on repeated runs."""
    fixture = Path(__file__).resolve().parent / 'resources/xml_shapes/typescript'
    lock = fixture / 'package-lock.json'
    packages = json.loads(lock.read_text(encoding='utf-8'))['packages']
    expected = {name.removeprefix('node_modules/'): value['version']
                for name, value in packages.items() if name.startswith('node_modules/')}
    modules = build / 'node_modules'

    def complete(directory):
        """Require every actual parser dependency and version selected by the maintained lock."""
        for name, version in expected.items():
            manifest = directory / name / 'package.json'
            if not manifest.is_file() or json.loads(manifest.read_text(encoding='utf-8'))['version'] != version:
                return False
        return True

    for name in ('package.json', 'package-lock.json'):
        shutil.copyfile(fixture / name, build / name)
    if not complete(modules):
        source = supplied.resolve(strict=True)
        if complete(source):
            for name in expected:
                shutil.copytree(source / name, modules / name, dirs_exist_ok=True)
        else:
            run(['npm.cmd' if os.name == 'nt' else 'npm', 'ci', '--prefer-offline', '--ignore-scripts',
                 '--no-audit', '--no-fund'], 'node-dependencies')
    if not complete(modules):
        raise RuntimeError('Actual Node parser dependencies do not match the maintained exact lock')
    evidence = {'lock_sha256': hashlib.sha256(lock.read_bytes()).hexdigest(), 'versions': expected}
    (build / 'node-dependencies.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    return modules
