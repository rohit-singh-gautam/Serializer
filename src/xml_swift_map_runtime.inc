// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"swmap(
// Independent per-wrapper policy; public XmlLimits retains its existing storage and initializer.
public struct XmlMapLimits {
    public let maxEntries: Int
    public let maxKeyBytes: Int
    // Retain operation-local bounds; generated entry points reject negative policies before use.
    public init(maxEntries: Int = 1024, maxKeyBytes: Int = 1048576) {
        self.maxEntries = maxEntries; self.maxKeyBytes = maxKeyBytes
    }
    // Reject invalid policies before a provider or output traversal starts retaining entries.
    fileprivate func validate() throws {
        guard maxEntries >= 0 && maxKeyBytes >= 0 else { throw SerializerError.invalid("Negative XML map limit") }
    }
    // Charge the larger lexical/canonical UTF8 key length without overflowing native counters.
    fileprivate func keyBytes(_ lexical: String, _ canonical: String, _ used: Int) throws -> Int {
        let count = max(lexical.utf8.count, canonical.utf8.count)
        guard used >= 0 && used <= maxKeyBytes && count <= maxKeyBytes - used else { throw SerializerError.invalid("XML map key byte limit exceeded") }
        return used + count
    }
}
)swmap"
