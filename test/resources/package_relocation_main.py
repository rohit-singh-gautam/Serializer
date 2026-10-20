# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Exercise the public generated Python package facade on independently authored OPC graphs."""
import copy
from dataclasses import replace
import json
from pathlib import Path
import sys

import schema as generated

A = generated.RelocationArchive


def main():
    """Require atomic failures, reusable preservation records and exact cumulative admission boundaries."""
    directory, name, action, expected = Path(sys.argv[1]), *sys.argv[2:]
    checks = 0

    def check(condition, message):
        """Keep acceptance assertions enabled regardless of Python optimization settings."""
        nonlocal checks
        if not condition:
            raise AssertionError(message)
        checks += 1

    sentinel = object()
    loaded = sentinel
    try:
        loaded = A.load((directory / (name + '.opc')).read_bytes(), policy=generated.ExtensionPolicy('preserve', 'preserve'))
    except generated.PackageError:
        check(expected == 'LOAD_ERROR' and loaded is sentinel, 'unexpected load failure or partial publication')
        print(json.dumps({'case': name, 'decision': expected, 'checks': checks, 'failure_stage': 'load'}))
        return
    check(expected != 'LOAD_ERROR', 'invalid duplicate origin published')
    source = loaded.value
    value = A.Input(source.first, None if action == 'remove_second' else source.second, source.children, loaded.extensions)
    original_extensions = copy.deepcopy(loaded.extensions)
    check(value.first.id == 11 and len(value.children) == 1 and value.children[0].source_index == 0, 'typed identity and child occurrence')
    destination = directory / (name + '-saved.opc')
    previous = b'previous destination'
    destination.write_bytes(previous)
    if expected == 'REJECT':
        for callback in (lambda: A.encode(value), lambda: A.save(destination, value)):
            try:
                callback()
                raise AssertionError('conflicting canonical graph published')
            except generated.PackageError:
                check(destination.read_bytes() == previous, 'rejected canonical Save changed destination')
        check(loaded.extensions == original_extensions, 'failed canonicalization mutated retained extensions')
        print(json.dumps({'case': name, 'decision': expected, 'checks': checks, 'failure_stage': 'encode/save'}))
        return
    measured = generated.PackageBudget()
    (directory / (name + '-encoded.opc')).write_bytes(A.encode(value, budget=measured))
    (directory / (name + '-repeat.opc')).write_bytes(A.encode(value))
    A.save(destination, value)
    check(loaded.extensions == original_extensions, 'repeat encode/Save mutated retained extension records')
    if name == 'alternate':
        for resource, field in (('work_units', 'max_work_units'), ('decoder_allocation_bytes', 'max_decoder_allocation_bytes')):
            used = measured.used[resource]
            check(used > 0, 'shared resource was not admitted')
            for tight in (False, True):
                limits = replace(generated.PackageLimits(), **{field: used - int(tight)})
                try:
                    A.encode(value, limits=limits)
                    check(not tight, 'one-tight canonical policy accepted')
                except generated.PackageError as failure:
                    check(tight and failure.code == 'resource_limit', 'exact policy or native resource classification')
        marker = RuntimeError('original canonical cancellation')
        polls = 0

        def cancelled():
            """Fail after initial preparation so the original caller exception traverses the package operation."""
            nonlocal polls
            polls += 1
            if polls >= 7:
                raise marker
            return False

        old = directory / 'cancelled.opc'
        old.write_bytes(previous)
        try:
            A.save(old, value, limits=generated.PackageLimits(is_cancelled=cancelled))
            raise AssertionError('cancelled canonical Save published')
        except RuntimeError as failure:
            check(failure is marker and old.read_bytes() == previous, 'original cancellation identity or Save atomicity')
        # Public relocation records permit an independently authored conflicting origin.
        conflict = replace(loaded.extensions, relocations=(("/same.xml", "/left/A.xml"), ("/SAME.xml", "/right/B.xml")))
        bad = A.Input(source.first, source.second, source.children, conflict)
        try:
            A.encode(bad)
            raise AssertionError('conflicting authored origin accepted')
        except generated.PackageError:
            check(loaded.extensions == original_extensions, 'origin conflict mutated original extensions')
    print(json.dumps({'case': name, 'decision': expected, 'checks': checks,
                      'work_units': measured.used['work_units'],
                      'allocation_bytes': measured.used['decoder_allocation_bytes']}))


if __name__ == '__main__':
    main()
