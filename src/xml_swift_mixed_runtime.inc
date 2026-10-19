// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// Bound native fragment storage independently of ordinary XML and map policies.
public struct XmlMixedLimits {
    public let maxFragments: Int
    public let maxTextBytes: Int
    // Preserve exact zero bounds; operation entry points reject negative policies.
    public init(maxFragments: Int = 100000, maxTextBytes: Int = 8388608) {
        self.maxFragments = maxFragments; self.maxTextBytes = maxTextBytes
    }
    // Validate before any input or output candidate allocation.
    fileprivate func validate() throws {
        guard maxFragments >= 0 && maxTextBytes >= 0 else { throw SerializerError.invalid("Negative XML mixed limit") }
    }
}
// Retain counters for one sequence, sharing no mutable state between operations.
fileprivate final class SrlXmlMixedBudget {
    private let limits: XmlMixedLimits
    private var fragments = 0
    private var bytes = 0
    // Borrow the immutable admission policy of the owning complete operation.
    init(_ limits: XmlMixedLimits) { self.limits = limits }
    // Charge every authored fragment before retaining its text or child value.
    func fragment() throws {
        guard fragments < limits.maxFragments else { throw SerializerError.invalid("XML mixed fragment limit exceeded") }; fragments += 1
    }
    // Check exact Unicode scalar validity and cumulative UTF8 bytes before appending.
    func text(_ value: String) throws {
        let size = value.utf8.count
        guard size <= limits.maxTextBytes - bytes else { throw SerializerError.invalid("XML mixed text limit exceeded") }
        for scalar in value.unicodeScalars {
            let code = scalar.value
            guard code == 9 || code == 10 || code == 13 || (32...0xd7ff).contains(code) || (0xe000...0xfffd).contains(code) || (0x10000...0x10ffff).contains(code) else { throw SerializerError.invalid("Invalid XML character") }
        }
        bytes += size
    }
}
)serializer"
