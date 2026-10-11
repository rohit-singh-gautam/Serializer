import Foundation

let widgetDomain: UInt32 = 1
let gadgetDomain: UInt32 = 2

// Build an identity that remains meaningful after copying or restoring its owning value.
func id(_ scope: UInt32, _ domain: UInt32, _ local: UInt64) -> CustomId {
    var value = CustomId()
    value.scope = scope
    value.domain = domain
    value.local = local
    return value
}

// Compare the entire identity tuple instead of accepting a coincidentally equal local number.
func sameId(_ left: CustomId, _ right: CustomId) -> Bool {
    return left.scope == right.scope && left.domain == right.domain && left.local == right.local
}

// Admit nonzero scopes and local values in the application's two declared domains.
func validId(_ value: CustomId) -> Bool {
    return value.scope != 0 && value.local != 0 &&
        (value.domain == widgetDomain || value.domain == gadgetDomain)
}

// Own live bindings and durable retired identities; methods mutate only this registry value.
struct HostRegistry {
    private(set) var state: RegistryState

    // Validate decoded state before making its identities available to lookups.
    init(_ value: RegistryState) throws {
        var seen: [CustomId] = []
        for identity in value.bindings.map({ $0.id }) + value.retired {
            guard validId(identity) && !seen.contains(where: { sameId($0, identity) })
                else { throw SerializerError.invalid("Invalid or duplicate registry identity") }
            seen.append(identity)
        }
        state = value
    }

    // Locate only an exact live identity with the caller's expected domain tag.
    func locate(_ value: CustomId, _ expectedDomain: UInt32) -> Int? {
        guard validId(value) && value.domain == expectedDomain else { return nil }
        return state.bindings.firstIndex(where: { sameId($0.id, value) })
    }

    // Copy a fresh identity and reject both live duplicates and retired identity reuse.
    mutating func bind(_ value: CustomId, _ payload: UInt32) -> Bool {
        guard validId(value) && !state.bindings.contains(where: { sameId($0.id, value) }) &&
            !state.retired.contains(where: { sameId($0, value) }) else { return false }
        var binding = EntityBinding()
        binding.id = value
        binding.payload = payload
        state.bindings.append(binding)
        return true
    }

    // Return a payload by value without exposing mutable registry storage.
    func lookup(_ value: CustomId, _ expectedDomain: UInt32) -> UInt32? {
        guard let index = locate(value, expectedDomain) else { return nil }
        return state.bindings[index].payload
    }

    // Replace a live payload while preserving its durable identity.
    mutating func rebind(_ value: CustomId, _ expectedDomain: UInt32, _ payload: UInt32) -> Bool {
        guard let index = locate(value, expectedDomain) else { return false }
        state.bindings[index].payload = payload
        return true
    }

    // Remove a live binding and keep its identity permanently in serialized retired state.
    mutating func retire(_ value: CustomId, _ expectedDomain: UInt32) -> Bool {
        guard let index = locate(value, expectedDomain) else { return false }
        state.retired.append(state.bindings.remove(at: index).id)
        return true
    }
}

// Check tuple identity, policy failures and all four generated codec round trips.
let first = id(7, widgetDomain, 42)
let second = id(8, widgetDomain, 42)
let gadget = id(7, gadgetDomain, 42)
let copied = first
precondition(sameId(first, copied))
var registry = try HostRegistry(RegistryState())
precondition(registry.bind(first, 10) && registry.bind(second, 20) && registry.bind(gadget, 30))
precondition(!registry.bind(first, 99))
precondition(registry.lookup(first, widgetDomain) == 10 && registry.lookup(second, widgetDomain) == 20)
precondition(registry.lookup(gadget, gadgetDomain) == 30 && registry.lookup(first, gadgetDomain) == nil)
precondition(registry.lookup(id(9, widgetDomain, 42), widgetDomain) == nil)
precondition(registry.lookup(id(7, widgetDomain, 43), widgetDomain) == nil)
precondition(!registry.bind(id(0, widgetDomain, 42), 99) && !registry.bind(id(7, widgetDomain, 0), 99))
precondition(!registry.bind(id(7, 3, 42), 99))
precondition(registry.rebind(first, widgetDomain, 11) && registry.lookup(copied, widgetDomain) == 11)
precondition(registry.retire(first, widgetDomain))
precondition(registry.lookup(first, widgetDomain) == nil && !registry.rebind(first, widgetDomain, 99))
precondition(!registry.bind(first, 99))
let canonical = try registry.state.encode(.BINARY_NONE)
for protocolValue in Protocol.allCases {
    let decoded = try RegistryState.decode(registry.state.encode(protocolValue), protocolValue)
    let decodedCanonical = try decoded.encode(.BINARY_NONE)
    precondition(decodedCanonical == canonical)
    var restored = try HostRegistry(decoded)
    precondition(restored.lookup(second, widgetDomain) == 20 && restored.lookup(gadget, gadgetDomain) == 30)
    precondition(!restored.bind(first, 99))
}
var duplicate = registry.state
duplicate.bindings.append(duplicate.bindings[0])
if let _ = try? HostRegistry(duplicate) { fatalError("Duplicate decoded identity accepted") }
var reused = registry.state
var forbidden = EntityBinding()
forbidden.id = first
forbidden.payload = 99
reused.bindings.append(forbidden)
if let _ = try? HostRegistry(reused) { fatalError("Decoded retired identity reused") }
print("Scoped custom ID registry passed four protocols")
