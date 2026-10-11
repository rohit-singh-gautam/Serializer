private const val widgetDomain: UInt = 1u
private const val gadgetDomain: UInt = 2u

// Construct an application-owned identity independently of receiver addresses and sequence positions.
private fun id(scope: UInt, domain: UInt, local: ULong): CustomId {
    val value = CustomId()
    value.scope = scope
    value.domain = domain
    value.local = local
    return value
}

// Copy every identity component so authored objects cannot mutate durable registry keys.
private fun copyId(value: CustomId): CustomId = id(value.scope, value.domain, value.local)

// Compare all three identity components, preserving scope and domain separation.
private fun sameId(left: CustomId, right: CustomId): Boolean =
    left.scope == right.scope && left.domain == right.domain && left.local == right.local

// Validate the application's nonzero scope/local and recognized domain policy.
private fun validId(value: CustomId): Boolean = value.scope != 0u && value.local != 0uL &&
    (value.domain == widgetDomain || value.domain == gadgetDomain)

// Own bindings and tombstones with policy validation whenever decoded state is restored.
private class HostRegistry(val state: RegistryState) {
    init {
        val seen = mutableSetOf<Triple<UInt, UInt, ULong>>()
        for (identity in state.bindings.map { it.id } + state.retired) {
            require(validId(identity) && seen.add(Triple(identity.scope, identity.domain, identity.local))) {
                "Invalid or duplicate registry identity"
            }
        }
    }

    // Locate an exact live identity only when its domain matches the requested entity kind.
    private fun locate(value: CustomId, expectedDomain: UInt): Int? {
        if (!validId(value) || value.domain != expectedDomain) return null
        return state.bindings.indexOfFirst { sameId(it.id, value) }.takeIf { it >= 0 }
    }

    // Copy a new binding and reject live duplicates or reuse of durable retired identities.
    fun bind(value: CustomId, payload: UInt): Boolean {
        if (!validId(value) || state.bindings.any { sameId(it.id, value) } ||
            state.retired.any { sameId(it, value) }) return false
        val binding = EntityBinding()
        binding.id = copyId(value)
        binding.payload = payload
        state.bindings.add(binding)
        return true
    }

    // Read payload state without returning mutable storage owned by the registry.
    fun lookup(value: CustomId, expectedDomain: UInt): UInt? =
        locate(value, expectedDomain)?.let { state.bindings[it].payload }

    // Replace only a live binding's payload, retaining its complete durable identity.
    fun rebind(value: CustomId, expectedDomain: UInt, payload: UInt): Boolean {
        val index = locate(value, expectedDomain) ?: return false
        state.bindings[index].payload = payload
        return true
    }

    // Persist the identity as a tombstone while removing its active binding.
    fun retire(value: CustomId, expectedDomain: UInt): Boolean {
        val index = locate(value, expectedDomain) ?: return false
        state.retired.add(copyId(state.bindings.removeAt(index).id))
        return true
    }
}

// Exercise copied IDs, tuple lookup, durable retirement and every generated codec.
fun main() {
    val first = id(7u, widgetDomain, 42uL)
    val second = id(8u, widgetDomain, 42uL)
    val gadget = id(7u, gadgetDomain, 42uL)
    val copied = copyId(first)
    check(sameId(copied, first))
    val registry = HostRegistry(RegistryState())
    check(registry.bind(first, 10u) && registry.bind(second, 20u) && registry.bind(gadget, 30u))
    check(!registry.bind(first, 99u))
    check(registry.lookup(first, widgetDomain) == 10u && registry.lookup(second, widgetDomain) == 20u)
    check(registry.lookup(gadget, gadgetDomain) == 30u && registry.lookup(first, gadgetDomain) == null)
    check(registry.lookup(id(9u, widgetDomain, 42uL), widgetDomain) == null)
    check(registry.lookup(id(7u, widgetDomain, 43uL), widgetDomain) == null)
    check(!registry.bind(id(0u, widgetDomain, 42uL), 99u) && !registry.bind(id(7u, widgetDomain, 0uL), 99u))
    check(!registry.bind(id(7u, 3u, 42uL), 99u))
    check(registry.rebind(first, widgetDomain, 11u) && registry.lookup(copied, widgetDomain) == 11u)
    check(registry.retire(first, widgetDomain))
    check(registry.lookup(first, widgetDomain) == null && !registry.rebind(first, widgetDomain, 99u))
    check(!registry.bind(first, 99u))
    val canonical = registry.state.encode(Protocol.BINARY_NONE)
    for (protocol in Protocol.entries) {
        val decoded = RegistryState.decode(registry.state.encode(protocol), protocol)
        check(decoded.encode(Protocol.BINARY_NONE).contentEquals(canonical))
        val restored = HostRegistry(decoded)
        check(restored.lookup(second, widgetDomain) == 20u && restored.lookup(gadget, gadgetDomain) == 30u)
        check(!restored.bind(first, 99u))
    }
    val duplicate = RegistryState.decode(canonical, Protocol.BINARY_NONE)
    duplicate.bindings.add(duplicate.bindings[0])
    check(runCatching { HostRegistry(duplicate) }.isFailure)
    val reused = RegistryState.decode(canonical, Protocol.BINARY_NONE)
    val forbidden = EntityBinding()
    forbidden.id = copyId(first)
    forbidden.payload = 99u
    reused.bindings.add(forbidden)
    check(runCatching { HostRegistry(reused) }.isFailure)
    println("Scoped custom ID registry passed four protocols")
}
