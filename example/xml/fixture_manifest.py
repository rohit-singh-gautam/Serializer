# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Read the shared XML producer/consumer inventory without silently omitting SDKs."""
import json
from pathlib import Path, PurePosixPath
import xml.etree.ElementTree as ET

LANGUAGE_PREFIXES = {
    'cpp': 'CP', 'python': 'PY', 'java': 'JV', 'kotlin': 'KT', 'csharp': 'CS',
    'go': 'GO', 'javascript': 'JS', 'typescript': 'TS', 'rust': 'RS',
    'swift': 'SW', 'c': 'C',
}
LANGUAGE_CONSUMERS = {
    'cpp': 'main.cpp', 'python': 'main.py', 'java': 'Main.java', 'kotlin': 'Main.kt',
    'csharp': 'Program.cs', 'go': 'main.go', 'javascript': 'main.mjs',
    'typescript': 'main.mts', 'rust': 'main.rs', 'swift': 'main.swift', 'c': 'main.c',
}
MAX_MANIFEST_BYTES = 64 * 1024
MAX_DOCUMENT_BYTES = 16 * 1024 * 1024


def source_path(root, spelling):
    """Resolve an existing portable source path while rejecting traversal and outside symlinks."""
    if not isinstance(spelling, str) or not spelling or '\\' in spelling:
        raise ValueError('Fixture source paths must be nonempty relative POSIX paths')
    path = PurePosixPath(spelling)
    if path.is_absolute() or any(part in ('', '.', '..') or ':' in part for part in spelling.split('/')):
        raise ValueError('Fixture source paths must stay within the repository')
    destination = (root / path).resolve()
    if not destination.is_relative_to(root.resolve()) or not destination.is_file():
        raise ValueError(f'Fixture source is missing or outside the repository: {spelling}')
    return destination


def load_manifest(path, root):
    """Require one bounded fixture and exactly eleven distinct maintained language consumers."""
    with Path(path).open('rb') as source:
        raw = source.read(MAX_MANIFEST_BYTES + 1)
    if len(raw) > MAX_MANIFEST_BYTES:
        raise ValueError('XML fixture manifest exceeds its byte limit')
    def distinct_pairs(pairs):
        """Reject duplicate JSON keys rather than discarding part of the fixture contract."""
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f'Duplicate manifest property: {key}')
            result[key] = value
        return result
    value = json.loads(raw, object_pairs_hook=distinct_pairs)
    required = {'version', 'id', 'schema', 'config', 'input', 'expected', 'exchange', 'languages'}
    if not isinstance(value, dict) or set(value) != required or type(value['version']) is not int or value['version'] != 1:
        raise ValueError('Unsupported XML fixture manifest structure/version')
    if not isinstance(value['id'], str) or not value['id'] or value['exchange'] != 'all_producers_to_all_consumers':
        raise ValueError('XML fixture requires an identity and every producer/consumer pair')
    for key in ('schema', 'config', 'input'):
        source_path(root, value[key])
    expected = value['expected']
    if not isinstance(expected, dict) or set(expected) != {'root', 'attributes', 'children'}:
        raise ValueError('XML fixture requires exact root, attributes and ordered children')
    if not isinstance(expected['root'], str) or not expected['root'] or not isinstance(expected['attributes'], dict):
        raise ValueError('Invalid XML fixture root/attributes')
    if any(not isinstance(key, str) or not isinstance(item, str) for key, item in expected['attributes'].items()):
        raise ValueError('XML fixture attribute names/values must be strings')
    if not isinstance(expected['children'], list) or any(
            not isinstance(item, list) or len(item) != 2 or
            any(not isinstance(part, str) for part in item) for item in expected['children']):
        raise ValueError('XML fixture children must be ordered name/value pairs')
    if not isinstance(value['languages'], list):
        raise ValueError('XML fixture languages must be an explicit inventory')
    seen = set()
    for entry in value['languages']:
        if not isinstance(entry, dict) or set(entry) != {'id', 'prefix', 'source', 'tools'}:
            raise ValueError('Invalid XML fixture language entry')
        language = entry['id']
        if not isinstance(language, str) or language in seen or LANGUAGE_PREFIXES.get(language) != entry['prefix']:
            raise ValueError('Unknown, duplicate or incorrectly prefixed XML fixture language')
        seen.add(language)
        source_path(root, entry['source'])
        if entry['source'] != f'example/xml/{language}/{LANGUAGE_CONSUMERS[language]}':
            raise ValueError('XML consumer source must name the maintained SDK entry point')
        if not isinstance(entry['tools'], list) or any(
                not isinstance(tool, str) or not tool or not tool.isascii() or
                not all(character.isalnum() or character in '_-' for character in tool)
                for tool in entry['tools']) or len(set(entry['tools'])) != len(entry['tools']):
            raise ValueError('Fixture SDK tools must be distinct executable names')
    if seen != set(LANGUAGE_PREFIXES):
        raise ValueError('XML fixture manifest must represent all eleven languages')
    check_document(source_path(root, value['input']), expected)
    return value


def check_document(path, expected):
    """Use an independent XML parser to enforce the shared expanded-name/value oracle."""
    with Path(path).open('rb') as source:
        raw = source.read(MAX_DOCUMENT_BYTES + 1)
    if len(raw) > MAX_DOCUMENT_BYTES or b'<!DOCTYPE' in raw:
        raise AssertionError(f'Invalid exported XML extent: {path}')
    raw.decode('utf-8')
    element = ET.fromstring(raw)
    if element.tag != expected['root'] or element.attrib != expected['attributes']:
        raise AssertionError(f'XML root/attribute mapping differs: {path}')
    children = [[child.tag, child.text or ''] for child in element]
    if children != expected['children'] or any(list(child) or child.attrib for child in element):
        raise AssertionError(f'XML ordered child values differ: {path}')
    if (element.text or '').strip() or any((child.tail or '').strip() for child in element):
        raise AssertionError(f'Unexpected XML mixed content: {path}')


def require_sdks(manifest, languages, runner, run):
    """Probe each selected real SDK; unresolved, missing or failing tools abort the exchange."""
    entries = {entry['id']: entry for entry in manifest['languages']}
    for language in languages:
        for tool in entries[language]['tools']:
            switch = ('version' if tool == 'go' else
                      '-version' if tool in ('java', 'javac', 'kotlinc') else '--version')
            command = runner.command(language, tool, switch)
            run(command)
