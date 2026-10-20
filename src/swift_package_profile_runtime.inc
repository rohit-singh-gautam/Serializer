// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packageprofile(
extension @package_name@ {
    public struct Role {
        public let field: Swift.String
        public let name: Swift.String?
        public let contentType: Swift.String?
        public let source: Swift.String
        public let relationship: Swift.String
        public let id: Swift.String?
        public let compression: Compression
        public let minimum: Swift.Int
        public let maximum: Swift.Int?
        public let fixed: Swift.Bool
        fileprivate let expectedRoot: SrlXmlName?
        fileprivate let expectedType: SrlXmlName?
        /// Immutable generated role/type dispatch is independent of runtime compression selection.
        public init(_ field: Swift.String, name: Swift.String?, contentType: Swift.String?, source: Swift.String, relationship: Swift.String, id: Swift.String?, compression: Compression, minimum: Swift.Int, maximum: Swift.Int?, fixed: Swift.Bool = false, expectedRoot: Swift.String? = nil, expectedType: Swift.String? = nil) {
            self.field = field; self.name = name; self.contentType = contentType; self.source = source
            self.relationship = relationship; self.id = id; self.compression = compression
            self.minimum = minimum; self.maximum = maximum; self.fixed = fixed
            self.expectedRoot = expectedRoot.map(expanded); self.expectedType = expectedType.map(expanded)
        }
    }
    public struct CodecContext {
        public let budget: Budget
        public let role: Swift.String
        public let part: Swift.String
        public let relationshipId: Swift.String
        fileprivate let definition: Role
        /// Keep exact logical codec location without exposing payload text.
        fileprivate init(_ budget: Budget, _ role: Role, _ part: Swift.String, _ id: Swift.String) {
            self.budget = budget; self.role = role.field; self.part = part; relationshipId = id; definition = role
        }
        /// Share cooperative cancellation with a synchronous native/provider callback.
        public func check() throws { try budget.check() }
        /// Charge field work/reference/allocation before aggregate publication.
        public func consume(_ resource: Resource, _ amount: Swift.Int, field: Swift.String = "") throws {
            try budget.consumeAnnotated(resource,amount,part:part,field:role+(field.isEmpty ? "":"."+field),reference:"",relationshipId:relationshipId)
        }
    }
    /// Exact UTF8 key identities; compiler validation keeps role key/reference scalar types equal.
    public final class References {
        private struct Key: Swift.Hashable { let role: Swift.String; let scope: WireString; let value: WireString }
        private struct Identity: Swift.Hashable { let scope: WireString; let value: WireString }
        private let budget: Budget
        private var keys: [Identity:Swift.String] = [:]
        private var required: [(Key,Swift.String,Swift.String,Swift.String)] = []
        /// Borrow the same cumulative operation ledger as every nested XML codec.
        public init(_ budget: Budget) { self.budget = budget }
        /// Preserve the selected reference field/scope when shared admission fails; caller errors pass through.
        private func charge(_ value: Swift.String, _ scope: Swift.String, _ part: Swift.String, _ field: Swift.String, _ relationshipId: Swift.String) throws {
            try budget.consumeAnnotated(.references,1,part:part,field:field,reference:scope,relationshipId:relationshipId)
            try budget.consumeAnnotated(.decoderAllocationBytes,value.utf8.count,part:part,field:field,reference:scope,relationshipId:relationshipId)
        }
        /// Reject duplicate target identities even when individual XML parts validate independently.
        public func add(_ role: Swift.String, scope: Swift.String, value: Swift.String, part: Swift.String, field: Swift.String, relationshipId: Swift.String = "") throws {
            try charge(value,scope,part,field,relationshipId)
            let identity = Identity(scope:WireString(scope),value:WireString(value))
            guard keys[identity] == nil else { throw Failure(.semantic,"Duplicate package key",part:part,fieldPath:field,reference:scope,relationshipId:relationshipId) }; keys[identity] = role
        }
        /// Retain forward references until every target role has been collected.
        public func require(_ role: Swift.String, scope: Swift.String, value: Swift.String, part: Swift.String, field: Swift.String, relationshipId: Swift.String = "") throws {
            try charge(value,scope,part,field,relationshipId)
            required.append((Key(role:role,scope:WireString(scope),value:WireString(value)),part,field,relationshipId))
        }
        /// Publish no aggregate when any scoped target is missing or has another model identity.
        public func finish() throws {
            for (key,part,field,link) in required {
                try budget.consumeAnnotated(.workUnits,1,part:part,field:field,reference:key.scope.value,relationshipId:link)
                guard keys[Identity(scope:key.scope,value:key.value)] == key.role else { throw Failure(.semantic,"Missing package reference target",part:part,fieldPath:field,reference:key.scope.value,relationshipId:link) }
            }
        }
    }
    /// Validate complete role cardinality with a named field before any caller result is returned.
    private static func occurs(_ role: Role, _ count: Swift.Int) throws {
        guard count >= role.minimum, role.maximum == nil || count <= role.maximum! else { throw Failure(.semantic,"Package role cardinality failed",part:role.field) }
    }
    private enum IdentityStop: Swift.Error { case accepted }
    /// Inspect only the typed root's expanded identities before entering an application codec callback.
    private static func checkIdentity(_ bytes: [Swift.UInt8], _ context: CodecContext) throws {
        guard let expected = context.definition.expectedRoot else { return }
        let limits = xmlLimits(context); try srlXmlTokenPreflight(bytes,limits); var accepted = false
        let handler = SrlXmlTokenParser(bytes,limits) { name,attributes in
            guard name == expected else { throw SerializerError.invalid("Unexpected package XML root") }
            let metadata = try srlXmlVariantType(attributes,false)
            guard metadata.identity == nil || metadata.identity == context.definition.expectedType else { throw SerializerError.invalid("Unexpected package XML type") }
            accepted = true; throw IdentityStop.accepted
        }
        let parser = XMLParser(data:Foundation.Data(bytes)); parser.delegate = handler
        parser.shouldProcessNamespaces = true; parser.shouldReportNamespacePrefixes = true; parser.shouldResolveExternalEntities = false
        _ = parser.parse()
        guard accepted, handler.failure is IdentityStop else { throw handler.failure ?? parser.parserError ?? SerializerError.invalid("Missing package XML root") }
    }
    /// Rebase moved-role links from original context without mutating preserved records or payloads.
    private static func relocatedRelationships(_ extensions: Extensions, _ sources: [Swift.String:[Swift.String]], _ budget: Budget) throws -> [Relationship] {
        guard !extensions.relocations.isEmpty else { return extensions.relationships }
        var destinations: Swift.Set<Swift.String> = []
        let destinationIndexEntryBytes = Swift.MemoryLayout<Swift.String>.stride + 2 * Swift.MemoryLayout<Swift.Int>.stride
        for (origin,move) in extensions.relocations {
            try budget.consume(.workUnits,1,part:origin)
            try budget.consume(.workUnits,origin.utf8.count,part:origin)
            try budget.consume(.workUnits,move.destination.utf8.count,part:origin)
            try partName(origin); try partName(move.destination)
            // Admit the owned folded string and logical set entry before retaining either.
            try budget.consume(.decoderAllocationBytes,destinationIndexEntryBytes,part:origin)
            try budget.consume(.decoderAllocationBytes,move.destination.utf8.count,part:origin)
            guard destinations.insert(fold(move.destination)).inserted else { throw Failure(.duplicateName,"Conflicting canonical role destinations",part:move.destination) }
        }
        var result: [Relationship] = []
        for original in extensions.relationships {
            try budget.consumeAnnotated(.workUnits,1,part:original.source,field:"relationships",reference:"",relationshipId:original.id)
            try budget.consume(.workUnits,original.source.utf8.count,part:original.source)
            try budget.consume(.workUnits,original.target.utf8.count,part:original.source)
            let sourceMove = extensions.relocations[fold(original.source)]
            if let move = sourceMove, !(sources[move.role]?.contains(where: { fold($0) == fold(move.destination) }) ?? false) {
                throw Failure(.missingPart,"Preserved source role was removed",part:original.source,relationshipId:original.id)
            }
            let source = sourceMove?.destination ?? original.source
            var target = original.target
            if !original.external {
                // Resolve both endpoints against the original graph before applying any simultaneous move.
                try budget.consume(.decoderAllocationBytes,original.source.utf8.count,part:original.source)
                try budget.consume(.decoderAllocationBytes,original.target.utf8.count,part:original.source)
                let resolved = try resolve(original.source,original.target)
                let targetMove = extensions.relocations[fold(resolved)]
                if let move = targetMove, !(sources[move.role]?.contains(where: { fold($0) == fold(move.destination) }) ?? false) {
                    throw Failure(.missingPart,"Preserved target role was removed",part:resolved,relationshipId:original.id)
                }
                if targetMove != nil || !source.utf8.elementsEqual(original.source.utf8) {
                    let destination = targetMove?.destination ?? resolved
                    let marker = selectorIndex(original.target)
                    let suffix = original.target[marker...]
                    try budget.consume(.decoderAllocationBytes,destination.utf8.count,part:original.source)
                    try budget.consume(.decoderAllocationBytes,suffix.utf8.count,part:original.source)
                    target = destination; target.append(contentsOf:suffix)
                }
            }
            try budget.consumeAnnotated(.decoderAllocationBytes,Swift.MemoryLayout<Relationship>.stride,part:original.source,field:"relationships",reference:"",relationshipId:original.id)
            result.append(Relationship(source:source,id:original.id,type:original.type,target:target,external:original.external))
        }
        return result
    }
    /// Synchronous unpublished composition stages typed values and inert opaque parts together.
    public final class Composer {
        private let budget: Budget
        private let profiles: [Swift.String:Compression]
        private var selected: Swift.Set<Swift.String> = []
        private var sources: [Swift.String:[Swift.String]] = ["/":["/"]]
        private var parts: [BinaryPart] = []
        private var relationships: [Relationship] = []
        private var busy = false
        private var closed = false
        /// Retain runtime compression choices; unknown roles reject at completion.
        public init(budget: Budget, profiles: [Swift.String:Compression] = [:]) { self.budget = budget; self.profiles = profiles }
        /// Fail closed on synchronous recursive entry or previously failed composition.
        private func begin(_ role: Role) throws {
            guard !busy, !closed, selected.insert(role.field).inserted else { closed = true; throw Failure(.invalidState,"Closed, repeated or reentrant package composer") }
            busy = true; try budget.check()
        }
        /// Resolve declared parent occurrence before codec work or relationship construction.
        private func parent(_ role: Role, _ index: Swift.Int) throws -> Swift.String {
            guard let parents = sources[role.source], index >= 0, index < parents.count else { throw Failure(.invalidRelationship,"Missing package role parent",part:role.field) }; return parents[index]
        }
        /// Apply storage override while preserving a fixed profile's complete policy.
        private func compression(_ role: Role) throws -> Compression {
            let value = profiles[role.field] ?? role.compression
            guard !role.fixed || value == role.compression else { throw Failure(.invalidOptions,"Fixed package profile cannot change",part:role.field) }; return value
        }
        /// Retain one validated payload and its compiler-owned source/type relationship.
        private func add(_ role: Role, _ data: [Swift.UInt8], _ name: Swift.String, _ type: Swift.String, _ sourceIndex: Swift.Int, _ index: Swift.Int) throws -> Swift.String {
            try budget.check(); guard !closed else { throw Failure(.invalidState,"Closed package composer") }
            let selectedName = role.name ?? name, selectedType = role.contentType ?? type
            try partName(selectedName); _ = try media(selectedType)
            let source = try parent(role,sourceIndex), id = role.id ?? "srl_"+role.field+"_"+Swift.String(index)
)packageprofile"
R"packageprofile(            parts.append(BinaryPart(data,name:selectedName,contentType:selectedType,sourceIndex:sourceIndex,compression:try compression(role)))
            relationships.append(Relationship(source:source,id:id,type:role.relationship,target:selectedName)); return selectedName
        }
        /// Compose an optional fixed model through the same bounded repeated-role implementation.
        public func single<T>(_ role: Role, _ value: T?, encoder: (T,CodecContext) throws -> [Swift.UInt8]) throws {
            try models(role,value.map { [ModelPart($0,name:role.name ?? "",contentType:role.contentType ?? "")] } ?? [],encoder:encoder)
        }
        /// A failed middle model leaves the entire composition unpublished and permanently fenced.
        public func models<T>(_ role: Role, _ values: [ModelPart<T>], encoder: (T,CodecContext) throws -> [Swift.UInt8]) throws {
            try begin(role); defer { busy = false }
            do {
                try occurs(role,values.count); var discovered: [Swift.String] = []
                for (index,item) in values.enumerated() {
                    _ = try parent(role,item.sourceIndex); let name = role.name ?? item.name
                    let context = CodecContext(budget,role,name,role.id ?? "srl_"+role.field+"_"+Swift.String(index))
                    let data: [Swift.UInt8]
                    do { data = try encoder(item.value,context); try checkIdentity(data,context) } catch let failure as Failure { throw failure } catch { try budget.check(); throw Failure(.codec,"Package typed XML encoder rejected part",part:name,fieldPath:role.field,relationshipId:context.relationshipId) }
                    try context.check(); try budget.consume(.decoderAllocationBytes,data.count,part:name)
                    discovered.append(try add(role,data,item.name,item.contentType,item.sourceIndex,index))
                }
                sources[role.field] = discovered
            } catch { closed = true; throw error }
        }
        /// Compose opaque bytes without interpreting attachments as XML or fetching any links.
        public func binaries(_ role: Role, _ values: [BinaryPart]) throws {
            try begin(role); defer { busy = false }
            do { try occurs(role,values.count); var discovered: [Swift.String] = []; for (index,item) in values.enumerated() { discovered.append(try add(role,item.data,item.name,item.contentType,item.sourceIndex,index)) }; sources[role.field] = discovered }
            catch { closed = true; throw error }
        }
        /// Revalidate preserved edges against edited parts before returning complete canonical bytes.
        public func finish(_ extensions: Extensions = Extensions()) throws -> [Swift.UInt8] {
            guard !closed, !busy else { closed = true; throw Failure(.invalidState,"Closed package composer") }; busy = true; defer { busy = false; closed = true; parts = []; relationships = [] }
            guard profiles.keys.allSatisfy({ selected.contains($0) }) else { throw Failure(.invalidOptions,"Unknown package profile role") }
            for part in extensions.parts { try budget.consume(.preservedBytes,part.data.count,part:part.name) }
            let preserved = try relocatedRelationships(extensions,sources,budget)
            return try opcWrite(parts+extensions.parts,relationships:relationships+preserved,budget:budget)
        }
    }
    /// Own a private catalog and decoded role inventory until final aggregate policy validation.
    public final class Loader {
        private let budget: Budget
        private let archive: ZipReader
        private let opc: Opc
        private var sources: [Swift.String:[Swift.String]] = ["/":["/"]]
        private var selectedParts: Swift.Set<Swift.String> = []
        private var selectedLinks: Swift.Set<Swift.Int> = []
        private var aliases: [Swift.String:Relocation] = [:]
        private var locations: [Swift.String:[(part:Swift.String,relationshipId:Swift.String)]] = [:]
        private var selectedRoles: Swift.Set<Swift.String> = []
        private var busy = false
        private var closed = false
        /// Parse infrastructure transactionally without invoking a typed decoder until dispatch agrees.
        public init(_ bytes: [Swift.UInt8], budget: Budget) throws {
            self.budget = budget; archive = try ZipReader.open(bytes,budget:budget)
            do { opc = try Opc(archive) } catch { archive.close(); throw error }
        }
        private struct Occurrence { let metadata: PartMetadata; let parent: Swift.Int; let id: Swift.String }
        /// Route by declared source/type/media; canonical output paths never select input codec types.
        private func discover(_ role: Role) throws -> [Occurrence] {
            guard !busy, !closed, selectedRoles.insert(role.field).inserted else { closed = true; throw Failure(.invalidState,"Closed, repeated or reentrant package loader") }; busy = true
            guard let parents = sources[role.source] else { throw Failure(.invalidRelationship,"Package source role unavailable",part:role.field) }
            var values: [Occurrence] = []
            for (parentIndex,parent) in parents.enumerated() {
                for (index,link) in opc.relationships.enumerated() where fold(link.source) == fold(parent) && link.type.utf8.elementsEqual(role.relationship.utf8) {
                    guard !link.external else { throw Failure(.invalidRelationship,"Typed role cannot use external relationship",part:role.field) }
                    let target = try resolve(link.source,link.target)
                    guard let part = opc.parts.first(where: { fold($0.name) == fold(target) }) else { throw Failure(.missingPart,"Missing role payload",part:target) }
                    if let expected = role.contentType { guard try media(part.contentType) == media(expected) else { throw Failure(.semantic,"Package role content type differs",part:target) } }
                    guard !role.fixed || part.entry.compression == role.compression else { throw Failure(.invalidOptions,"Fixed package compression differs",part:target,fieldPath:role.field) }
                    guard selectedParts.insert(fold(part.name)).inserted else { throw Failure(.semantic,"Part selected by multiple roles",part:target) }
                    selectedLinks.insert(index); values.append(Occurrence(metadata:part,parent:parentIndex,id:link.id))
                    if let canonical = role.name, !part.name.utf8.elementsEqual(canonical.utf8) {
                        try budget.consume(.workUnits,1,part:part.name)
                        try budget.consume(.decoderAllocationBytes,part.name.utf8.count,part:part.name)
                        try budget.consume(.decoderAllocationBytes,Swift.MemoryLayout<Relocation>.stride,part:part.name)
                        aliases[fold(part.name)] = Relocation(destination:canonical,role:role.field)
                    }
                }
            }
            try occurs(role,values.count); sources[role.field] = values.map { $0.metadata.name }
            locations[role.field] = values.map { ($0.metadata.name,$0.id) }; return values
        }
        /// Return the actual discovered source relationship for post-load reference diagnostics.
        public func location(_ role: Swift.String, _ index: Swift.Int) -> (part:Swift.String,relationshipId:Swift.String)? {
            guard let values = locations[role], index >= 0, index < values.count else { return nil }; return values[index]
        }
        /// Decode a fixed optional model only after immutable role media/type dispatch succeeds.
        public func single<T>(_ role: Role, decoder: ([Swift.UInt8],CodecContext) throws -> T) throws -> T? {
            let values = try models(role,decoder:decoder); guard values.count <= 1 else { throw Failure(.semantic,"Singleton role has multiple values") }; return values.first?.value
        }
        /// Keep all middle-part results local until the generated complete aggregate validates.
        public func models<T>(_ role: Role, decoder: ([Swift.UInt8],CodecContext) throws -> T) throws -> [ModelPart<T>] {
            defer { busy = false }
            do {
                let occurrences = try discover(role); var values: [ModelPart<T>] = []
                for item in occurrences {
                    let context = CodecContext(budget,role,item.metadata.name,item.id); try context.check()
                    let bytes = try opc.read(item.metadata.name); let value: T
                    do { try checkIdentity(bytes,context); value = try decoder(bytes,context) } catch let failure as Failure { throw failure } catch { try budget.check(); throw Failure(.codec,"Package typed XML decoder rejected part",part:item.metadata.name,fieldPath:role.field,relationshipId:context.relationshipId) }
                    try context.check(); guard !closed else { throw Failure(.invalidState,"Closed package loader") }
                    values.append(ModelPart(value,name:item.metadata.name,contentType:item.metadata.contentType,sourceIndex:item.parent))
                }
                return values
            } catch { closed = true; throw error }
        }
        /// Return independent checked opaque bytes using declared source-scoped role dispatch.
        public func binaries(_ role: Role) throws -> [BinaryPart] {
            defer { busy = false }
            do { let occurrences = try discover(role); return try occurrences.map { item in BinaryPart(try opc.read(item.metadata.name),name:item.metadata.name,contentType:item.metadata.contentType,sourceIndex:item.parent,compression:item.metadata.entry.compression) } }
            catch { closed = true; throw error }
        }
        /// Preserve whole bytes and original link context; canonical relocation waits for edited composition.
        public func finish(_ policy: ExtensionPolicy = ExtensionPolicy()) throws -> Extensions {
)packageprofile"
R"packageprofile(            guard !busy, !closed else { closed = true; throw Failure(.invalidState,"Closed package loader") }; busy = true; defer { busy = false; closed = true }
            var parts: [BinaryPart] = [], links: [Relationship] = []
            for part in opc.parts where !selectedParts.contains(fold(part.name)) {
                try budget.check(); let required = try policy.requiredPart?(part) ?? false
                guard !required, policy.parts != .reject else { throw Failure(.unsupportedFeature,"Unknown or required package part",part:part.name) }
                if policy.parts == .preserve { let bytes = try opc.read(part.name); try budget.consume(.preservedBytes,bytes.count,part:part.name); parts.append(BinaryPart(bytes,name:part.name,contentType:part.contentType,compression:part.entry.compression)) }
            }
            for (index,link) in opc.relationships.enumerated() where !selectedLinks.contains(index) {
                try budget.check(); let required = try policy.requiredRelationship?(link) ?? false
                guard !required, policy.relationships != .reject else { throw Failure(.unsupportedFeature,"Unknown or required package relationship",part:link.source) }
                if policy.relationships == .preserve {
                    for text in [link.source,link.id,link.type,link.target] { try budget.consume(.preservedBytes,text.utf8.count,part:link.source) }
                    links.append(link)
                }
            }
            try budget.check(); return Extensions(parts:parts,relationships:links,relocations:aliases)
        }
        /// Release only the private archive; decoded and preserved values remain independently owned.
        public func close() { closed = true; archive.close() }
    }
    /// Revalidate complete bytes with the same ledger before Foundation's atomic replacement primitive.
    public static func save(_ bytes: [Swift.UInt8], to path: Foundation.URL, budget: Budget) throws {
        let reader = try ZipReader.open(bytes,budget:budget); defer { reader.close() }
        for entry in reader.entries { _ = try reader.read(entry.name) }
        try budget.check(); try Foundation.Data(bytes).write(to:path,options:.atomic); try budget.check()
    }
}
extension @package_name@.Budget {
    /// Annotate only local admission failures; original policy/cancellation errors retain their identity.
    fileprivate func consumeAnnotated(_ resource: @package_name@.Resource, _ amount: Swift.Int, part: Swift.String, field: Swift.String, reference: Swift.String, relationshipId: Swift.String) throws {
        try check()
        guard amount >= 0, amount <= remaining(resource) else { throw @package_name@.Failure(.resourceLimit,"Package resource limit exceeded",part:part,fieldPath:field,reference:reference,relationshipId:relationshipId) }
        used[resource.rawValue] += amount
    }
}
)packageprofile"
