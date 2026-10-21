// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packageiri(
extension @package_name@ {
    /// Check RFC 3987 Unicode ranges, admitting private scalars only in the query component.
    private static func iriScalar(_ value: Swift.UInt32, _ query: Swift.Bool) -> Swift.Bool {
        (0xa0...0xd7ff).contains(value) || (0xf900...0xfdcf).contains(value) || (0xfdf0...0xffef).contains(value) ||
        ((0x10000...0xdfffd).contains(value) && value & 0xffff <= 0xfffd) || (0xe1000...0xefffd).contains(value) ||
        query && ((0xe000...0xf8ff).contains(value) || (0xf0000...0xffffd).contains(value) || (0x100000...0x10fffd).contains(value))
    }
    /// Validate exact IRI component delimiters and percent triplets without rewriting authored text.
    private static func iriComponent(_ value: Substring, _ extra: Swift.String, query: Swift.Bool = false) -> Swift.Bool {
        let scalars = value.unicodeScalars; var index = scalars.startIndex
        while index != scalars.endIndex {
            let scalar = scalars[index]; index = scalars.index(after:index)
            if scalar.value == 37 {
                for _ in 0..<2 { guard index != scalars.endIndex, scalars[index].value < 128, hex(Swift.UInt8(scalars[index].value)) != nil else { return false }; index = scalars.index(after:index) }
            } else if scalar.value >= 128 { if !iriScalar(scalar.value,query) { return false } }
            else if !unreserved(Swift.UInt8(scalar.value)) && !"!$&'()*+,;=".unicodeScalars.contains(scalar) && !extra.unicodeScalars.contains(scalar) { return false }
        }
        return true
    }
    /// Validate a decimal IPv4 tail without accepting legacy octal or abbreviated addresses.
    private static func iriIpv4(_ value: Substring) -> Swift.Bool {
        let pieces = value.split(separator:".",omittingEmptySubsequences:false)
        return pieces.count == 4 && pieces.allSatisfy { !$0.isEmpty && $0.utf8.count <= 3 && ($0.count == 1 || !$0.hasPrefix("0")) && $0.utf8.allSatisfy { $0 >= 48 && $0 <= 57 } && (Swift.Int($0) ?? 256) <= 255 }
    }
    /// Recognize RFC IP literals without resolving addresses or consulting networking services.
    private static func iriIp(_ value: Substring) -> Swift.Bool {
        if value.hasPrefix("v") || value.hasPrefix("V") {
            guard let dot = value.firstIndex(of:".") else { return false }
            let version = value[value.index(after:value.startIndex)..<dot], address = value[value.index(after:dot)...]
            return !version.isEmpty && version.utf8.allSatisfy { hex($0) != nil } && !address.isEmpty && address.utf8.allSatisfy { unreserved($0) || "!$&'()*+,;=:".utf8.contains($0) }
        }
        guard value.utf8.count <= 45, value.utf8.allSatisfy({ $0 < 128 }) else { return false }
        let text = Swift.String(value), compressed = text.range(of:"::")
        if text.contains(":::") { return false }
        if let compressed, text[compressed.upperBound...].contains("::") { return false }
        let pieces = text.split(separator:":",omittingEmptySubsequences:false); var groups = 0
        for (index,piece) in pieces.enumerated() {
            if piece.isEmpty { if compressed == nil { return false }; continue }
            if piece.contains(".") { if index != pieces.count-1 || !iriIpv4(piece) { return false }; groups += 2 }
            else { if piece.utf8.count > 4 || !piece.utf8.allSatisfy({hex($0) != nil}) { return false }; groups += 1 }
        }
        if text.hasPrefix(":") && !text.hasPrefix("::") || text.hasSuffix(":") && !text.hasSuffix("::") { return false }
        return compressed == nil ? groups == 8 : groups < 8
    }
    /// Separate user-info, host and port so reserved delimiters cannot masquerade as host text.
    private static func iriAuthority(_ input: Substring) -> Swift.Bool {
        var value = input
        if let at = value.unicodeScalars.firstIndex(of:"@") { guard iriComponent(value[..<at],":") else { return false }; value = value[value.unicodeScalars.index(after:at)...] }
        let port: Substring
        if value.hasPrefix("[") {
            guard let end = value.firstIndex(of:"]"), iriIp(value[value.index(after:value.startIndex)..<end]) else { return false }
            let rest = value[value.index(after:end)...]; guard rest.isEmpty || rest.hasPrefix(":") else { return false }; port = rest.isEmpty ? rest : rest.dropFirst()
        } else if let colon = value.unicodeScalars.firstIndex(of:":") { guard iriComponent(value[..<colon],"") else { return false }; port = value[value.unicodeScalars.index(after:colon)...] }
        else { return iriComponent(value,"") }
        return port.utf8.allSatisfy { $0 >= 48 && $0 <= 57 }
    }
    /// Apply RFC 3986/3987 component grammar; relationship types additionally require an absolute fragment-free IRI.
    private static func validIri(_ input: Swift.String, absolute: Swift.Bool = false) -> Swift.Bool {
        var value = input[...]
        if let hash = value.unicodeScalars.firstIndex(of:"#") { if absolute || !iriComponent(value[value.unicodeScalars.index(after:hash)...],":@/?") { return false }; value = value[..<hash] }
        if let query = value.unicodeScalars.firstIndex(of:"?") { if !iriComponent(value[value.unicodeScalars.index(after:query)...],":@/?",query:true) { return false }; value = value[..<query] }
        if let colon = value.unicodeScalars.firstIndex(of:":"), colon < (value.unicodeScalars.firstIndex(of:"/") ?? value.endIndex) {
            let scheme = value[..<colon]
            guard !scheme.isEmpty, scheme.utf8.enumerated().allSatisfy({ index,byte in byte >= 65 && byte <= 90 || byte >= 97 && byte <= 122 || index > 0 && (byte >= 48 && byte <= 57 || "+-.".utf8.contains(byte)) }) else { return false }; value = value[value.unicodeScalars.index(after:colon)...]
        } else if absolute { return false }
        if value.hasPrefix("//") { let rest = value.dropFirst(2); let slash = rest.unicodeScalars.firstIndex(of:"/") ?? rest.endIndex; guard iriAuthority(rest[..<slash]) else { return false }; value = rest[slash...] }
        return iriComponent(value,":@/")
    }
    /// Match the OPC ST_Extension grammar, including percent triplets and its selected punctuation.
    private static func validExtension(_ input: Swift.String) -> Swift.Bool {
        guard !input.isEmpty else { return false }; let bytes = Swift.Array(input.utf8); var index = 0
        while index < bytes.count { let byte = bytes[index]; index += 1
            if byte == 37 { guard bytes.count-index >= 2, hex(bytes[index]) != nil, hex(bytes[index+1]) != nil else { return false }; index += 2 }
            else if byte == 46 || !unreserved(byte) && !"!$&'()*+,:=@".utf8.contains(byte) { return false }
        }; return true
    }
    /// Normalize only XSD anyURI and ID whitespace, retaining XML string attribute semantics elsewhere.
    private static func uriAttribute(_ node: SrlXmlNode, _ name: Swift.String, _ budget: Budget) throws -> Swift.String {
        let value = try attribute(node,name); try budget.consume(.workUnits,value.utf8.count); try budget.consume(.decoderAllocationBytes,value.utf8.count)
        var normalized = Swift.String(); normalized.reserveCapacity(value.utf8.count); var separator = false
        for scalar in value.unicodeScalars {
            if scalar.value == 32 || scalar.value == 9 || scalar.value == 13 || scalar.value == 10 { separator = !normalized.isEmpty; continue }
            if separator { normalized.append(" "); separator = false }; normalized.unicodeScalars.append(scalar)
        }
        return normalized
    }
    /// Exclude physical nonparts while diagnosing the deliberately unsupported Unicode logical naming profile.
    private static func mappedName(_ input: Swift.String) throws -> Swift.String? {
        let bytes = Swift.Array(input.utf8)
        for (index,byte) in bytes.enumerated() {
            if byte >= 128 || byte == 37 && bytes.count-index >= 3 && hex(bytes[index+1]) != nil && hex(bytes[index+2]) != nil && hex(bytes[index+1])! >= 8 { throw Failure(.unsupportedFeature,"Non-ASCII OPC logical naming is unsupported",part:input) }
        }
        do { return try partName(input,infrastructure:true) } catch { return nil }
    }
}
)packageiri"
