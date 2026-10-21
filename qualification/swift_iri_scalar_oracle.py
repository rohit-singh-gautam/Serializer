# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Compile the exact generated Swift IRI helper and check scalar-delimiter and IP-literal boundaries."""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess


def helper(source):
    """Extract emitted helper functions without substituting a Python URI implementation."""
    text = source.read_text(encoding='utf-8')
    start = text.index('    private static func hex(')
    end = text.index('    /// Check selected URI', start)
    hexadecimal = text[start:end]
    start = text.index('    private static func unreserved(')
    end = text.index('\n', start)
    unreserved = text[start:end]
    start = text.index('    private static func iriScalar(')
    end = text.index('    /// Match the OPC ST_Extension', start)
    return hexadecimal + unreserved + '\n' + text[start:end]


def main():
    """Require current rejection and a sensitive old-helper counterfactual in the actual Swift SDK."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--counterfactual-source', required=True, type=Path)
    parser.add_argument('--swiftc', default='swiftc')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if hasattr(ctypes, 'windll'):
        ctypes.windll.kernel32.SetErrorMode(0x8003)
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[1]
    current = output / 'Schema.swift'
    subprocess.run([str(args.compiler.resolve()), '--input', str(root / 'test/resources/opc_profile.serializer'),
                    '--language', 'swift', '--output', str(current)], check=True, capture_output=True)
    bad = ('urn:\U000e0100value', 'https://user@\U000e0100host.invalid/',
           'https://host.invalid/?\U000e0100', 'https://host.invalid/#\U000e0100',
           'https://[:::]/','https://[1:::2]/','https://[:::1]/','https://[1:::]/')
    literals = json.dumps(bad, ensure_ascii=False).replace('\U000e0100', r'\u{e0100}')
    results = {}
    for name, source in [('counterfactual', args.counterfactual_source), ('current', current)]:
        text = 'import Foundation\nenum Probe {\n' + helper(source) + '\n'
        text += '    static func run() { if !validIri("https://[v1.:::]/") { print([-1]); return }; let inputs = ' + literals + '; print(inputs.enumerated().filter { validIri($0.element) }.map { $0.offset }) }\n}\nProbe.run()\n'
        path = output / (name + '.swift'); path.write_text(text, encoding='utf-8')
        executable = output / (name + ('.exe' if hasattr(ctypes, 'windll') else ''))
        built = subprocess.run([args.swiftc, '-O', '-warnings-as-errors', str(path), '-o', str(executable)], capture_output=True, text=True)
        (output / (name + '-compile.log')).write_text(built.stdout + built.stderr, encoding='utf-8')
        built.check_returncode()
        result = subprocess.run([str(executable)], check=True, capture_output=True, text=True)
        (output / (name + '-run.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        results[name] = json.loads(result.stdout)
    assert results['counterfactual'] == list(range(len(bad))), 'Counterfactual must accept all eight invalid forms and the valid IPvFuture address'
    assert results['current'] == [], results
    report = {'status':'passed', 'cases':len(bad), 'positive_cases':1,
              'ipvfuture_positive':'https://[v1.:::]/', 'accepted_indices':results,
              'compiler_sha256':hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              'generated_sha256':hashlib.sha256(current.read_bytes()).hexdigest(),
              'swift_version':subprocess.check_output([args.swiftc,'--version'],text=True)}
    (output / 'verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()
