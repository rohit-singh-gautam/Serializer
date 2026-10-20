// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"scalarpython(
class XmlPatternLimits:
    """Bound immutable portable pattern programs independently of optional providers."""
    def __init__(self, max_states=16384, max_input_bytes=8 * 1024 * 1024,
                 max_work_units=64 * 1024 * 1024):
        """Validate explicit nonnegative admission bounds; equality remains permitted."""
        for key, value in locals().copy().items():
            if key == 'self':
                continue
            if type(value) is not int or value < 0:
                raise ValueError('Invalid XML pattern limit: ' + key)
            setattr(self, key, value)


class _XmlPatternContext:
    """Borrow application callbacks while carrying an operation-local pattern policy."""
    def __init__(self, context, policy):
        """Preserve callback ownership and exceptions throughout one synchronous operation."""
        self.context, self.xml_pattern = context, policy
    def __getattr__(self, name):
        """Preserve optional shared package diagnostics and reference admission."""
        return getattr(self.context, name)
    def consume(self, resource, count):
        """Delegate cumulative charging without swallowing application failures."""
        if self.context is not None:
            self.context.consume(resource, count)
    def check(self):
        """Delegate interruption checks at the provider boundary."""
        if self.context is not None:
            self.context.check()
    def check_buffer(self, amount):
        """Forward an available shared-buffer policy without requiring one for scalar-only contexts."""
        callback = getattr(self.context, 'check_buffer', None)
        if callback is not None:
            callback(amount)


def _xml_pattern_context(limits, context):
    """Install only an explicitly supplied policy; avoid nested wrappers for shared budgets."""
    policy = getattr(limits, 'xml_pattern', None)
    if policy is None or getattr(context, 'xml_pattern', None) is policy:
        return context
    if not isinstance(policy, XmlPatternLimits):
        raise ValueError('Expected XmlPatternLimits')
    return _XmlPatternContext(context, policy)


def _xml_portable_pattern(value, program, context=None):
    """Run the compiler's Thompson program with bounded closures and Unicode scalar matching."""
    limits = getattr(context, 'xml_pattern', None) or XmlPatternLimits()
    start, states = program
    # Avoid an allocation proportional to input bytes until the admission test succeeds.
    if len(value) > limits.max_input_bytes or len(states) > limits.max_states:
        _xml_limit(context, 'Portable XML pattern input/state limit exceeded')
    work = 0
    def charge(count=1):
        """Charge every closure/state/range visit before performing the corresponding work."""
        nonlocal work
        if count > limits.max_work_units - work:
            _xml_limit(context, 'Portable XML pattern work limit exceeded')
        work += count
        if context is not None:
            context.consume('work_units', count)
    charge(len(value))
    size = 0
    for character in value:
        code = ord(character)
        size += 1 if code < 0x80 else 2 if code < 0x800 else 3 if code < 0x10000 else 4
        if size > limits.max_input_bytes:
            _xml_limit(context, 'Portable XML pattern input/state limit exceeded')
    _xml_check_text(value)
    if context is not None:
        context.consume('decoder_allocation_bytes', len(states) * 40)
    visited, stack, generation = [0] * len(states), [], 0
    def closure(selected):
        """Visit each state once, including epsilon cycles introduced by nullable repetition."""
        nonlocal generation
        charge(len(states))
        generation += 1
        stack.extend(selected)
        selected.clear()
        while stack:
            charge()
            index = stack.pop()
            if visited[index] == generation:
                continue
            visited[index] = generation
            state = states[index]
            if state[3] or state[6]:
                selected.append(index)
            stack.extend(state[0])
    active, selected = [start], []
    closure(active)
    for character in value:
        charge()
        code = ord(character)
        selected.clear()
        for index in active:
            charge()
            state = states[index]
            if not state[3]:
                continue
            if state[5]:
                matched = character not in ('\n', '\r')
            else:
                matched = False
                for first, last in state[1]:
                    charge()
                    matched = matched or first <= code <= last
                if state[4]:
                    matched = not matched
            if matched:
                selected.append(state[2])
        closure(selected)
        active, selected = selected, active
    return any(states[index][6] for index in active)

)scalarpython"
