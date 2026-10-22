# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Consume independent package fixtures through generated Python profile APIs."""
from dataclasses import replace
import json
from pathlib import Path
import sys
import schema as generated

A = generated.PipelineArchive
directory, name, codec, decision = Path(sys.argv[1]), *sys.argv[2:]
profiles = {'main': {'codec': codec}}
checks = 0


def check(condition, message):
    """Retain assertions in optimized and ordinary interpreter invocations."""
    global checks
    if not condition:
        raise AssertionError(message)
    checks += 1


try:
    decode_budget = generated.PackageBudget()
    source_bytes = (directory / (name + '.opc')).read_bytes()
    loaded = A.load(source_bytes, profiles=profiles, budget=decode_budget)
except generated.PackageError:
    check(decision == 'LOAD_ERROR', 'unexpected load rejection')
    print(json.dumps({'case': name, 'decision': decision, 'checks': checks}))
    sys.exit(0)
check(decision == 'OK', 'invalid input published')
for resource, field in (('work_units', 'max_work_units'), ('decoder_allocation_bytes', 'max_decoder_allocation_bytes')):
    used = decode_budget.used[resource]
    for allowance in (used, used - 1):
        try:
            A.load(source_bytes, profiles=profiles, limits=replace(generated.PackageLimits(), **{field: allowance}))
            check(allowance == used, 'one-tight decode quota accepted')
        except generated.PackageError as failure:
            check(allowance != used and failure.code == 'resource_limit', 'exact decode quota or failure classification')
value = loaded.value
check(value.main.id == 17 and value.main.label == 'pipeline' and list(value.main.values) == [2, 4, 8], 'native model changed')
check(len(value.links) == 1 and value.links[0].content_type == '' and value.links[0].data == b'', 'external role is not inert')
input = A.Input(value.main, value.fixed, value.links, loaded.extensions, profiles)
observed = generated.PackageBudget()
(directory / (name + '-encoded.opc')).write_bytes(A.encode(input, budget=observed))
A.save(directory / (name + '-saved.opc'), input)
for resource, field in (('work_units', 'max_work_units'), ('decoder_allocation_bytes', 'max_decoder_allocation_bytes')):
    used = observed.used[resource]
    check(used > 0, 'missing resource admission')
    for allowance in (used, used - 1):
        try:
            A.encode(input, limits=replace(generated.PackageLimits(), **{field: allowance}))
            check(allowance == used, 'one-tight quota accepted')
        except generated.PackageError as failure:
            check(allowance != used and failure.code == 'resource_limit', 'exact quota or failure classification')
for override in ({'fixed': {'codec': 'binary_none'}}, {'fixed': 'deflate'}, {'missing': 'stored'}):
    try:
        A.select_profiles(override)
        raise AssertionError('invalid fixed/unknown selection accepted')
    except generated.PackageError:
        checks += 1
bad = replace(value.links[0], data=b'forbidden')
destination = directory / (name + '-atomic.opc')
destination.write_bytes(b'previous destination')
try:
    A.save(destination, A.Input(value.main, value.fixed, [bad], loaded.extensions, profiles))
    raise AssertionError('external payload published')
except generated.PackageError:
    check(destination.read_bytes() == b'previous destination', 'failed Save replaced destination')
marker = ValueError('original caller policy failure')
armed = False


def cancelled():
    """Throw once the original shared policy is invoked inside native part traversal."""
    if armed:
        raise marker
    return False


class CallerBudget(generated.PackageBudget):
    """Arm the public policy through native work admission without altering model codec methods."""
    def consume(self, resource, amount, part=''):
        """Borrow the same counters while arranging one caller-owned exception at part work."""
        global armed
        if part == '/main.data' and resource == 'work_units':
            armed = True
        return super().consume(resource, amount, part)


shared = CallerBudget(replace(generated.PackageLimits(), is_cancelled=cancelled))
try:
    A.save(destination, input, budget=shared)
    raise AssertionError('caller policy failure published')
except ValueError as failure:
    check(failure is marker and destination.read_bytes() == b'previous destination', 'caller exception identity or Save rollback lost')
print(json.dumps({'case': name, 'decision': decision, 'checks': checks}))
