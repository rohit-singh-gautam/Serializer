// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packageopc(
extension @package_name@ {
    private static let contentNamespace = "http://schemas.openxmlformats.org/package/2006/content-types"
    private static let relationshipNamespace = "http://schemas.openxmlformats.org/package/2006/relationships"
    private static let relationshipMime = "application/vnd.openxmlformats-package.relationships+xml"
    public struct BinaryPart {
        public let data: [Swift.UInt8]
        public let name: Swift.String
        public let contentType: Swift.String
        public let sourceIndex: Swift.Int
        public let compression: Compression
        /// Retain opaque owning values without interpreting or normalizing their payloads.
        public init(_ data: [Swift.UInt8], name: Swift.String, contentType: Swift.String, sourceIndex: Swift.Int = 0, compression: Compression = .stored) {
            self.data = data; self.name = name; self.contentType = contentType; self.sourceIndex = sourceIndex; self.compression = compression
        }
    }
    public struct ModelPart<T> {
        public let value: T
        public let name: Swift.String
        public let contentType: Swift.String
        public let sourceIndex: Swift.Int
        /// Dynamic typed occurrences retain actual source-scoped names independently of native values.
        public init(_ value: T, name: Swift.String, contentType: Swift.String, sourceIndex: Swift.Int = 0) {
            self.value = value; self.name = name; self.contentType = contentType; self.sourceIndex = sourceIndex
        }
    }
    public struct Relationship {
        public let source: Swift.String
        public let id: Swift.String
        public let type: Swift.String
        public let target: Swift.String
        public let external: Swift.Bool
        /// External destinations remain inert original text and are never fetched.
        public init(source: Swift.String, id: Swift.String, type: Swift.String, target: Swift.String, external: Swift.Bool = false) {
            self.source = source; self.id = id; self.type = type; self.target = target; self.external = external
        }
    }
    fileprivate struct Relocation { let destination: Swift.String; let role: Swift.String }
    public struct Extensions {
        public let parts: [BinaryPart]
        public let relationships: [Relationship]
        fileprivate let relocations: [Swift.String:Relocation]
        /// Preserve complete bytes and their source/target context without claiming lexical metadata preservation.
        public init(parts: [BinaryPart] = [], relationships: [Relationship] = []) { self.parts = parts; self.relationships = relationships; relocations = [:] }
        /// Keep loaded moved-role provenance private until the edited aggregate is composed.
        fileprivate init(parts: [BinaryPart], relationships: [Relationship], relocations: [Swift.String:Relocation]) {
            self.parts = parts; self.relationships = relationships; self.relocations = relocations
        }
    }
    public enum Unknown { case reject, skip, preserve }
    public struct ExtensionPolicy {
        public var parts: Unknown = .reject
        public var relationships: Unknown = .reject
        public var requiredPart: ((PartMetadata) throws -> Swift.Bool)?
        public var requiredRelationship: ((Relationship) throws -> Swift.Bool)?
        /// Require explicit caller policy for each unknown item and mandatory capability.
        public init() {}
    }
    public struct PartMetadata { public let name: Swift.String; public let contentType: Swift.String; fileprivate let entry: Entry }
    /// ASCII folding is the selected OPC URI identity policy, independent of Unicode canonical equivalence.
    private static func fold(_ text: Swift.String) -> Swift.String { text.lowercased() }
    /// Recognize exact ASCII hexadecimal digits without locale conversions.
    private static func hex(_ byte: Swift.UInt8) -> Swift.Int? {
        if byte >= 48 && byte <= 57 { return Swift.Int(byte-48) }; if byte >= 65 && byte <= 70 { return Swift.Int(byte-55) }; if byte >= 97 && byte <= 102 { return Swift.Int(byte-87) }; return nil
    }
    /// Check selected URI unreserved bytes independently of host URL normalization.
    private static func unreserved(_ value: Swift.UInt8) -> Swift.Bool { value >= 65 && value <= 90 || value >= 97 && value <= 122 || value >= 48 && value <= 57 || Swift.Array("-._~".utf8).contains(value) }
    /// Enforce the selected ASCII OPC part-name profile before names become identities.
    @discardableResult public static func partName(_ name: Swift.String, infrastructure: Swift.Bool = false) throws -> Swift.String {
        guard name.hasPrefix("/"), name.utf8.count > 1, name.utf8.count <= uint16Maximum+1 else { throw Failure(.invalidName, "Invalid OPC part URI", part: name) }
        if infrastructure && name == "/[Content_Types].xml" { return name }
        for segment in name.dropFirst().split(separator: "/", omittingEmptySubsequences: false) {
            guard !segment.isEmpty, !segment.hasSuffix(".") else { throw Failure(.invalidName, "Invalid OPC URI segment", part: name) }
            let bytes = Swift.Array(segment.utf8); var index = 0
            while index < bytes.count {
                let byte = bytes[index]; index += 1
                if byte == 37 {
                    guard bytes.count-index >= 2, let first = hex(bytes[index]), let second = hex(bytes[index+1]) else { throw Failure(.invalidName, "Malformed OPC escape", part: name) }
                    let decoded = Swift.UInt8(first*16+second); index += 2
                    guard decoded < 128, !unreserved(decoded), decoded != 47, decoded != 92 else { throw Failure(.invalidName, "Forbidden OPC escape", part: name) }
                } else { guard byte < 128, unreserved(byte) || Swift.Array("!$&'()*+,;=:@".utf8).contains(byte) else { throw Failure(.invalidName, "Invalid OPC URI character", part: name) } }
            }
        }
        if !infrastructure && (fold(name) == "/[content_types].xml" || relationshipSource(name) != nil) { throw Failure(.invalidName, "Reserved OPC infrastructure URI", part: name) }
        return name
    }
    /// Map infrastructure to its owner without recursively interpreting relationship parts.
    private static func relationshipSource(_ name: Swift.String) -> Swift.String? {
        if fold(name) == "/_rels/.rels" { return "/" }
        let pieces = name.split(separator: "/", omittingEmptySubsequences: false)
        guard pieces.count >= 3, fold(Swift.String(pieces[pieces.count-2])) == "_rels", fold(name).hasSuffix(".rels"), pieces.last!.count > 5 else { return nil }
        return pieces.dropLast(2).joined(separator: "/")+"/"+Swift.String(pieces.last!.dropLast(5))
    }
    /// Emit canonical physical metadata names for a previously validated payload owner.
    private static func relationshipName(_ source: Swift.String) -> Swift.String {
        if source == "/" { return "_rels/.rels" }
        let slash = source.lastIndex(of: "/")!
        return Swift.String(source[source.index(after: source.startIndex)..<source.index(after: slash)])+"_rels/"+Swift.String(source[source.index(after: slash)...])+".rels"
    }
    /// Admit complete media syntax and compare semantic parameter names without folding their values.
    private static func media(_ text: Swift.String) throws -> Swift.String {
        let bytes = Swift.Array(text.utf8); var position = 0
        while position < bytes.count && mimeToken(bytes[position]) { position += 1 }
        guard position > 0, position < bytes.count, bytes[position] == 47 else { throw Failure(.invalidMetadata,"Invalid OPC media type") }; position += 1
        let start = position
        while position < bytes.count && mimeToken(bytes[position]) { position += 1 }
        guard position > start else { throw Failure(.invalidMetadata,"Invalid OPC media subtype") }
        let head = fold(Swift.String(decoding:bytes[..<position],as:UTF8.self)); var parameters: [Swift.String:Swift.String] = [:]
        while position < bytes.count {
            while position < bytes.count && (bytes[position] == 32 || bytes[position] == 9) { position += 1 }
            guard position < bytes.count, bytes[position] == 59 else { throw Failure(.invalidMetadata,"Invalid MIME delimiter") }; position += 1
            while position < bytes.count && (bytes[position] == 32 || bytes[position] == 9) { position += 1 }
            let start = position
            while position < bytes.count && mimeToken(bytes[position]) { position += 1 }
            guard position > start, position < bytes.count, bytes[position] == 61 else { throw Failure(.invalidMetadata,"Invalid MIME parameter") }
            let name = fold(Swift.String(decoding:bytes[start..<position],as:UTF8.self)); position += 1; var value: [Swift.UInt8] = []
            if position < bytes.count && bytes[position] == 34 {
                position += 1; var ended = false
                while position < bytes.count {
                    var byte = bytes[position]; position += 1
                    if byte == 34 { ended = true; break }
                    if byte == 92 { guard position < bytes.count else { throw Failure(.invalidMetadata,"Unterminated MIME escape") }; byte = bytes[position]; position += 1 }
                    guard byte == 9 || byte >= 32 && byte <= 126 else { throw Failure(.invalidMetadata,"Invalid MIME quoted byte") }; value.append(byte)
                }
)packageopc"
R"packageopc(                guard ended else { throw Failure(.invalidMetadata,"Unterminated MIME quoted value") }
            } else {
                while position < bytes.count && mimeToken(bytes[position]) { value.append(bytes[position]); position += 1 }
                guard !value.isEmpty else { throw Failure(.invalidMetadata,"Empty MIME parameter") }
            }
            guard parameters[name] == nil else { throw Failure(.invalidMetadata,"Duplicate MIME parameter") }; parameters[name] = Swift.String(decoding:value,as:UTF8.self)
        }
        guard parameters.isEmpty || head != relationshipMime && head != "application/vnd.openxmlformats-package.core-properties+xml" else { throw Failure(.invalidMetadata,"Reserved content type parameters") }
        return head+parameters.keys.sorted().map { "\u{0}"+$0+"\u{0}"+parameters[$0]! }.joined()
    }
    /// Recognize MIME token bytes independently of locale or the host regular-expression dialect.
    private static func mimeToken(_ byte: Swift.UInt8) -> Swift.Bool { byte >= 48 && byte <= 57 || byte >= 65 && byte <= 90 || byte >= 97 && byte <= 122 || Swift.Array("!#$%&'*+-.^_`|~".utf8).contains(byte) }
    /// Validate inert absolute relationship type IRIs before any dispatch or publication.
    private static func relationshipType(_ value: Swift.String) throws {
        guard validIri(value,absolute:true) else { throw Failure(.invalidRelationship,"Invalid relationship type IRI") }
    }
    /// Find URI syntax by Unicode scalar so combining marks cannot hide ASCII delimiters.
    private static func selectorIndex(_ target: Swift.String) -> Swift.String.Index {
        target.unicodeScalars.firstIndex { $0 == "?" || $0 == "#" } ?? target.endIndex
    }
    /// Resolve internal relative targets inside the package root; external links are never consulted.
    public static func resolve(_ source: Swift.String, _ target: Swift.String) throws -> Swift.String {
        try iri(target)
        let path = target[..<selectorIndex(target)]
        if path.isEmpty && source != "/" { return try partName(source) }
        guard !path.isEmpty, !path.contains("\\"), !path.contains("//"), !path.hasSuffix("/"), !path.hasSuffix("/."), !path.hasSuffix("/.."), path != ".", path != "..",
            path.hasPrefix("/") || !(path.split(separator:"/").first?.contains(":") ?? false) else { throw Failure(.invalidRelationship, "Invalid internal relationship target") }
        var result: [Swift.String] = path.hasPrefix("/") ? [] : source.split(separator: "/").dropLast().map(Swift.String.init)
        for piece in path.split(separator: "/", omittingEmptySubsequences: false) {
            if piece.isEmpty || piece == "." { continue }; if piece == ".." { if !result.isEmpty { result.removeLast() } } else { result.append(Swift.String(piece)) }
        }
        do { return try partName("/"+result.joined(separator: "/")) } catch { throw Failure(.invalidRelationship, "Invalid resolved relationship target") }
    }
    /// Validate inert IRI reference characters and percent escapes without resolution or network access.
    private static func iri(_ value: Swift.String) throws {
        guard validIri(value) else { throw Failure(.invalidRelationship,"Invalid relationship IRI") }
    }
    /// Reject case aliases and ancestor collisions before retaining an OPC identity.
    private static func unique(_ name: Swift.String, _ names: inout Swift.Set<Swift.String>) throws {
        let key = fold(name)
        guard !names.contains(key) else { throw Failure(.duplicateName, "Duplicate OPC part identity", part: name) }
        guard !names.contains(where: { key.hasPrefix($0+"/") || $0.hasPrefix(key+"/") }) else { throw Failure(.invalidName, "OPC file/descendant collision", part: name) }
        names.insert(key)
    }
    /// Bind metadata XML to the same ledger and prohibit external schema/entity access.
    private static func metadata(_ bytes: [Swift.UInt8], _ budget: Budget, relationships: Swift.Bool = false) throws -> SrlXmlNode {
        try budget.consume(.metadataBytes, bytes.count)
        var limits = xmlLimits(budget)
        let compatibility = relationships ? RelationshipsCompatibility(budget) : nil
        if let compatibility { limits.srlPackageNamespaceScope = { node, lookup in try compatibility.capture(node,lookup) } }
        guard bytes.count <= budget.configured.maxPartBytes else { throw Failure(.resourceLimit,"Metadata part byte limit") }
        var normalized = bytes; var conversionHeld = 0
        defer { budget.release(conversionHeld) }
        if bytes.starts(with: [0xff,0xfe]) || bytes.starts(with: [0xfe,0xff]) {
            guard bytes.count % 2 == 0, bytes.count <= Swift.Int.max/3 else { throw Failure(.invalidMetadata,"Truncated UTF16 metadata") }
            let conversionBytes = bytes.count*3
            try budget.consume(.decoderAllocationBytes,conversionBytes); try budget.consume(.workUnits,bytes.count); try budget.hold(conversionBytes); conversionHeld = conversionBytes
            let encoding: Swift.String.Encoding = bytes.starts(with: [0xff,0xfe]) ? .utf16LittleEndian : .utf16BigEndian
            guard var text = Swift.String(data: Foundation.Data(bytes.dropFirst(2)), encoding: encoding) else { throw Failure(.invalidMetadata, "Invalid UTF16 package metadata") }
            if text.hasPrefix("<?xml"), let end = text.range(of:"?>") {
                let header = Swift.String(text[..<end.upperBound])
                if header.range(of:"\\bencoding\\s*=",options:.regularExpression) != nil {
                    guard let declaration = header.range(of:"\\bencoding\\s*=\\s*(['\"])UTF-16\\1",options:[.regularExpression,.caseInsensitive]),
                        let label = text.range(of:"UTF-16",options:.caseInsensitive,range:declaration) else { throw Failure(.invalidMetadata,"Conflicting UTF16 metadata encoding") }
                    text.replaceSubrange(label,with:"UTF-8")
                }
            }
            normalized = Swift.Array(text.utf8); limits.maxBytes = normalized.count
        }
        do {
            let root = try srlXmlParse(normalized, limits)
            if let compatibility { return try compatibility.apply(root) }
            return root
        } catch let failure as Failure { throw failure } catch { try budget.check(); throw Failure(.invalidMetadata, "Invalid package metadata XML") }
    }
    /// Carry typed and metadata tree admission into the cumulative package context.
    public static func xmlLimits(_ budget: Budget) -> XmlLimits {
        var limits = XmlLimits(); limits.maxBytes = budget.configured.maxPartBytes
        limits.maxTextBytes = budget.configured.maxPartBytes; limits.maxNodes = Swift.min(budget.remaining(.workUnits), 1000000)
        limits.srlPackageCheck = { try budget.check() }
        limits.srlPackageConsume = { kind, amount in try budget.consume(kind == "allocation" ? .decoderAllocationBytes : kind == "reference" ? .references : kind == "preserved" ? .preservedBytes : .workUnits, amount) }
        limits.srlPackageLimit = { message in Failure(.resourceLimit,message) }
        return limits
    }
    /// Attribute nested XML exhaustion to its declared role, physical part and selected relationship.
    public static func xmlLimits(_ context: CodecContext) -> XmlLimits {
        var limits = xmlLimits(context.budget)
        limits.srlPackageRootType = context.definition.expectedType
        limits.srlPackageConsume = { kind, amount in try context.consume(kind == "allocation" ? .decoderAllocationBytes : kind == "reference" ? .references : kind == "preserved" ? .preservedBytes : .workUnits,amount) }
        limits.srlPackageLimit = { message in Failure(.resourceLimit,message,part:context.part,fieldPath:context.role,relationshipId:context.relationshipId) }
        return limits
    }
    /// Decode compiler-owned expanded-name metadata without interpreting caller URI text.
    private static func expanded(_ value: Swift.String) -> SrlXmlName {
        if value.hasPrefix("{"), let end = value.firstIndex(of:"}") { return SrlXmlName(local:Swift.String(value[value.index(after:end)...]),namespace:Swift.String(value[value.index(after:value.startIndex)..<end])) }
        return SrlXmlName(local:value,namespace:"")
    }
    /// Consume every ordinary attribute exactly once; unknown core names always reject.
    private static func attribute(_ node: SrlXmlNode, _ name: Swift.String) throws -> Swift.String {
        guard let value = node.attributes.first(where: { $0.name.namespace.isEmpty && $0.name.local == name }) else { throw Failure(.invalidMetadata, "Missing package metadata attribute") }; return value.value
    }
    /// A complete catalog publishes after all content declarations and relationship targets validate.
    public final class Opc {
        public let archive: ZipReader
        public let parts: [PartMetadata]
        public let relationships: [Relationship]
        /// Own a catalog backed by the same bounded reader and no network capability.
        public init(_ archive: ZipReader) throws {
            self.archive = archive; let budget = archive.budget
            guard archive.entries.contains(where: { $0.name == "[Content_Types].xml" && $0.physicalFile }) else { throw Failure(.invalidMetadata, "Missing exact content-type manifest") }
            let root = try metadata(archive.read("[Content_Types].xml"), budget)
            guard root.name == SrlXmlName(local: "Types", namespace: contentNamespace), root.attributes.isEmpty, srlXmlWhitespace(root.text) else { throw Failure(.invalidMetadata, "Invalid content-type root") }
            var defaults: [Swift.String: Swift.String] = [:], overrides: [Swift.String: Swift.String] = [:]
            for child in root.children {
                guard child.name.namespace == contentNamespace, child.children.isEmpty, srlXmlWhitespace(child.text), child.attributes.count == 2, child.attributes.allSatisfy({ $0.name.namespace.isEmpty }) else { throw Failure(.invalidMetadata, "Invalid content-type declaration") }
                let value = try attribute(child,"ContentType"); _ = try media(value)
                if child.name.local == "Default" {
)packageopc"
R"packageopc(                    let ext = try attribute(child,"Extension"); guard validExtension(ext), defaults[fold(ext)] == nil else { throw Failure(.invalidMetadata, "Duplicate or invalid content-type default") }; defaults[fold(ext)] = value
                } else if child.name.local == "Override" {
                    let name = try uriAttribute(child,"PartName",budget); try partName(name,infrastructure: true); guard overrides[fold(name)] == nil else { throw Failure(.invalidMetadata, "Duplicate content-type override") }; overrides[fold(name)] = value
                } else { throw Failure(.invalidMetadata, "Unknown core content-type declaration") }
            }
            var names: Swift.Set<Swift.String> = []; var parts: [PartMetadata] = []; var metadataEntries: [(Swift.String,Entry)] = []
            for entry in archive.entries where entry.name != "[Content_Types].xml" {
                guard entry.physicalFile else { _ = try archive.read(entry.name); continue }
                guard let name = try mappedName("/"+entry.name) else { _ = try archive.read(entry.name); continue }; try unique(name,&names)
                let ext = entry.name.split(separator: ".").last.map(Swift.String.init) ?? ""
                guard let type = overrides[fold(name)] ?? defaults[fold(ext)] else { throw Failure(.invalidMetadata, "Missing OPC content type",part:name) }
                if let owner = relationshipSource(name) {
                    guard try media(type) == relationshipMime, owner == "/" || relationshipSource(owner) == nil else { throw Failure(.invalidMetadata, "Invalid relationship-part metadata") }; metadataEntries.append((owner,entry))
                } else { guard try media(type) != relationshipMime else { throw Failure(.invalidMetadata, "Relationship content type on payload") }; parts.append(PartMetadata(name:name,contentType:type,entry:entry)) }
            }
            for name in overrides.keys { guard names.contains(name) else { throw Failure(.invalidMetadata, "Content-type override has no part") } }
            var links: [Relationship] = []; var identifiers: Swift.Set<WireString> = []
            for (owner,entry) in metadataEntries {
                guard owner == "/" || parts.contains(where: { fold($0.name) == fold(owner) }) else { throw Failure(.invalidRelationship, "Relationship owner is missing",part:owner) }
                let root = try metadata(archive.read(entry.name),budget,relationships:true)
                guard root.name == SrlXmlName(local:"Relationships",namespace:relationshipNamespace), root.attributes.isEmpty, srlXmlWhitespace(root.text) else { throw Failure(.invalidRelationship,"Invalid relationships root") }
                for child in root.children {
                    guard child.name == SrlXmlName(local:"Relationship",namespace:relationshipNamespace), child.children.isEmpty, srlXmlWhitespace(child.text), child.attributes.allSatisfy({ $0.name.namespace.isEmpty && ["Id","Type","Target","TargetMode"].contains($0.name.local) }) else { throw Failure(.invalidRelationship,"Unknown core relationship markup") }
                    let id = try uriAttribute(child,"Id",budget), type = try uriAttribute(child,"Type",budget), target = try uriAttribute(child,"Target",budget)
                    try budget.consumeAnnotated(.relationships,1,part:owner,field:"relationships",reference:target,relationshipId:id)
                    try budget.consumeAnnotated(.references,1,part:owner,field:"relationships",reference:target,relationshipId:id)
                    guard srlXmlNcName(id), identifiers.insert(WireString(fold(owner)+"\n"+id)).inserted else { throw Failure(.invalidRelationship,"Invalid or duplicate relationship ID") }; try relationshipType(type)
                    let mode = child.attributes.first(where: { $0.name.local == "TargetMode" })?.value ?? "Internal"
                    guard mode == "Internal" || mode == "External" else { throw Failure(.invalidRelationship,"Invalid relationship target mode") }
                    if mode == "External" { try iri(target) }
                    else { let resolved = try resolve(owner,target); guard parts.contains(where: { fold($0.name) == fold(resolved) }) else { throw Failure(.invalidRelationship,"Dangling relationship target",part:resolved) } }
                    links.append(Relationship(source:owner,id:id,type:type,target:target,external:mode == "External"))
                }
            }
            self.parts = parts; relationships = links; try budget.check()
        }
        /// Read a checked payload identity through the same cumulative reader ledger.
        public func read(_ name: Swift.String) throws -> [Swift.UInt8] {
            guard let part = parts.first(where: { fold($0.name) == fold(name) }) else { throw Failure(.missingPart,"Missing OPC part",part:name) }; return try archive.read(part.entry.name)
        }
    }
    /// Escape metadata values while preserving relationship target spelling exactly.
    private static func escape(_ value: Swift.String) throws -> Swift.String { try srlXmlEscape(value,true) }
    /// Revalidate complete parts and links, then emit canonical infrastructure and sorted ZIP metadata.
    public static func opcWrite(_ parts: [BinaryPart], relationships: [Relationship], budget selected: Budget? = nil) throws -> [Swift.UInt8] {
        let budget = try selected ?? Budget(); var names: Swift.Set<Swift.String> = []; var types: [Swift.String:Swift.String] = [:]
        for part in parts { try budget.check(); try partName(part.name); try unique(part.name,&names); _ = try media(part.contentType); types[fold(part.name)] = part.contentType }
        var ids: Swift.Set<WireString> = []; var groups: [Swift.String:[Relationship]] = [:]
        for link in relationships {
            try budget.consumeAnnotated(.relationships,1,part:link.source,field:"relationships",reference:link.target,relationshipId:link.id)
            try budget.consumeAnnotated(.references,1,part:link.source,field:"relationships",reference:link.target,relationshipId:link.id); try relationshipType(link.type)
            guard link.source == "/" || names.contains(fold(link.source)), srlXmlNcName(link.id),
                ids.insert(WireString(fold(link.source)+"\n"+link.id)).inserted else { throw Failure(.invalidRelationship,"Invalid source or relationship ID") }
            if link.external { try iri(link.target) }
            else { guard names.contains(fold(try resolve(link.source,link.target))) else { throw Failure(.invalidRelationship,"Dangling edited relationship") } }
            groups[link.source,default:[]].append(link)
        }
        let writer = ZipWriter(opcBudget:budget); defer { writer.close() }
        var manifest = "<Types xmlns=\""+contentNamespace+"\"><Default Extension=\"rels\" ContentType=\""+relationshipMime+"\"/>"
        for part in parts.sorted(by: { $0.name < $1.name }) { manifest += "<Override PartName=\""+(try escape(part.name))+"\" ContentType=\""+(try escape(part.contentType))+"\"/>"; try writer.add(Swift.String(part.name.dropFirst()),part.data,compression:part.compression) }
        manifest += "</Types>"; try writer.add("[Content_Types].xml",Swift.Array(manifest.utf8))
        for (source,links) in groups {
            var xml = "<Relationships xmlns=\""+relationshipNamespace+"\">"
            for link in links.sorted(by: { $0.id.utf8.lexicographicallyPrecedes($1.id.utf8) }) { xml += "<Relationship Id=\""+(try escape(link.id))+"\" Type=\""+(try escape(link.type))+"\" Target=\""+(try escape(link.target))+"\""+(link.external ? " TargetMode=\"External\"":"")+"/>" }
            xml += "</Relationships>"; try writer.add(relationshipName(source),Swift.Array(xml.utf8))
        }
        return try writer.finish()
    }
}
)packageopc"
