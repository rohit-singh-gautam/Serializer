// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagemce(
extension @package_name@ {
    /// Apply Relationships compatibility without adding namespace snapshots to ordinary typed XML trees.
    private final class RelationshipsCompatibility {
        private static let namespace = "http://schemas.openxmlformats.org/markup-compatibility/2006"
        private static let xmlNamespace = "http://www.w3.org/XML/1998/namespace"
        private final class Scope {
            let parent: Scope?
            var ignored: Swift.Set<WireString> = []
            var process: Swift.Set<SrlXmlName> = []
            /// Link inherited rules without copying the accumulated vocabulary at every node.
            init(_ parent: Scope?) { self.parent = parent }
        }
        private let budget: Budget
        private var namespaces: [Swift.ObjectIdentifier:[WireString:Swift.String]] = [:]
        /// Borrow the same cumulative package context through selection and tree projection.
        init(_ budget: Budget) { self.budget = budget }
        /// Split only XML whitespace; nonbreaking spaces remain invalid tokens.
        private func tokens(_ value: Swift.String) throws -> [Swift.String] {
            try budget.consume(.workUnits,value.utf8.count+1)
            try budget.consume(.decoderAllocationBytes,value.utf8.count)
            let result = value.unicodeScalars.split { $0.value == 32 || $0.value == 9 || $0.value == 13 || $0.value == 10 }.map { Swift.String(Swift.String.UnicodeScalarView($0)) }
            try budget.consume(.decoderAllocationBytes,result.count*Swift.MemoryLayout<Swift.String>.stride)
            return result
        }
        /// Retain only prefixes referenced by compatibility directives in this original declaration scope.
        func capture(_ node: SrlXmlNode, _ lookup: (Swift.String) -> Swift.String?) throws {
            var captured: [WireString:Swift.String] = [:]
            for attribute in node.attributes {
                let compatibility = attribute.name.namespace == Self.namespace
                let requires = node.name.namespace == Self.namespace && node.name.local == "Choice" && attribute.name.namespace.isEmpty && attribute.name.local == "Requires"
                if !compatibility && !requires { continue }
                for token in try tokens(attribute.value) {
                    let prefix = compatibility && attribute.name.local == "ProcessContent" ? Swift.String(token.prefix { $0 != ":" }) : token
                    if let uri = lookup(prefix), captured[WireString(prefix)] == nil {
                        try budget.consume(.decoderAllocationBytes,prefix.utf8.count+uri.utf8.count+64)
                        captured[WireString(prefix)] = uri
                    }
                }
            }
            if !captured.isEmpty {
                try budget.consume(.decoderAllocationBytes,64)
                namespaces[Swift.ObjectIdentifier(node)] = captured
            }
        }
        /// Resolve a lexical prefix using only the scope captured while its owning start tag was active.
        private func prefix(_ node: SrlXmlNode, _ token: Swift.String) throws -> Swift.String {
            try budget.consume(.workUnits,token.utf8.count+1)
            guard srlXmlNcName(token), let uri = namespaces[Swift.ObjectIdentifier(node)]?[WireString(token)], !uri.isEmpty, uri != Self.namespace else { throw Failure(.invalidMetadata,"Invalid compatibility namespace prefix") }
            return uri
        }
        /// Search inherited URI identities under a finite shared work budget.
        private func ignored(_ scope: Scope?, _ uri: Swift.String) throws -> Swift.Bool {
            var current = scope
            while let value = current {
                try budget.consume(.workUnits,1)
                if value.ignored.contains(WireString(uri)) { return true }; current = value.parent
            }
            return false
        }
        /// Match exact expanded ProcessContent names and namespace wildcards.
        private func process(_ scope: Scope?, _ name: SrlXmlName) throws -> Swift.Bool {
            var current = scope
            while let value = current {
                try budget.consume(.workUnits,1)
                if value.process.contains(name) || value.process.contains(SrlXmlName(local:"*",namespace:name.namespace)) { return true }; current = value.parent
            }
            return false
        }
        /// Validate directives even on an unselected wrapper, while retaining only newly declared rules.
        private func declarations(_ node: SrlXmlNode, _ parent: Scope?) throws -> Scope {
            try budget.consume(.decoderAllocationBytes,128)
            let scope = Scope(parent)
            for attribute in node.attributes where attribute.name.namespace == Self.namespace && attribute.name.local == "Ignorable" {
                for token in try tokens(attribute.value) {
                    let uri = try prefix(node,token); try budget.consume(.decoderAllocationBytes,64); scope.ignored.insert(WireString(uri))
                }
            }
            for attribute in node.attributes where attribute.name.namespace == Self.namespace {
                switch attribute.name.local {
                case "Ignorable": break
                case "MustUnderstand": for token in try tokens(attribute.value) { _ = try prefix(node,token) }
                case "ProcessContent":
                    for token in try tokens(attribute.value) {
                        let parts = token.split(separator:":",omittingEmptySubsequences:false).map(Swift.String.init)
                        guard parts.count == 2, parts[1] == "*" || srlXmlNcName(parts[1]) else { throw Failure(.invalidMetadata,"Invalid ProcessContent name") }
                        let uri = try prefix(node,parts[0]); guard try ignored(scope,uri) else { throw Failure(.invalidMetadata,"ProcessContent namespace is not ignorable") }
                        try budget.consume(.decoderAllocationBytes,64); scope.process.insert(SrlXmlName(local:parts[1],namespace:uri))
                    }
                default: throw Failure(.invalidMetadata,"Unknown compatibility attribute")
                }
            }
            return scope
        }
        /// Check required capabilities only for content retained by the processing model.
        private func must(_ node: SrlXmlNode) throws {
            for attribute in node.attributes where attribute.name.namespace == Self.namespace && attribute.name.local == "MustUnderstand" {
                for token in try tokens(attribute.value) { guard try prefix(node,token) == relationshipNamespace else { throw Failure(.invalidMetadata,"Unsupported required compatibility namespace") } }
            }
        }
        /// Validate Choice/Fallback attributes before selecting the first understood branch.
        private func wrapper(_ node: SrlXmlNode, _ scope: Scope, _ choice: Swift.Bool) throws -> Swift.Bool {
            for attribute in node.attributes {
                if attribute.name.namespace.isEmpty {
                    guard choice && attribute.name.local == "Requires" else { throw Failure(.invalidMetadata,"Unexpected compatibility wrapper attribute") }
                } else if attribute.name.namespace != Self.namespace {
                    guard attribute.name.namespace != Self.xmlNamespace else { throw Failure(.invalidMetadata,"XML context on compatibility wrapper") }
                    guard try ignored(scope,attribute.name.namespace) else { throw Failure(.invalidMetadata,"Unexpected qualified compatibility wrapper attribute") }
                }
            }
            if !choice { return false }
            guard let requires = node.attributes.first(where: { $0.name.namespace.isEmpty && $0.name.local == "Requires" }) else { throw Failure(.invalidMetadata,"Choice requires namespace prefixes") }
            let required = try tokens(requires.value); guard !required.isEmpty else { throw Failure(.invalidMetadata,"Choice requires namespace prefixes") }
            var understood = true
            for token in required { if try prefix(node,token) != relationshipNamespace { understood = false } }
            return understood
        }
)packagemce"
R"packagemce(
        /// Flatten selected children while retaining any nonwhitespace text for final OPC grammar rejection.
        private func children(_ node: SrlXmlNode, _ scope: Scope?) throws {
            var result: [SrlXmlNode] = []; var text = node.text
            for child in node.children {
                let action = try visit(child,scope,false)
                if action == 0 { try budget.consume(.decoderAllocationBytes,Swift.MemoryLayout<SrlXmlNode>.stride); result.append(child) }
                else if action == 1 {
                    try budget.consume(.decoderAllocationBytes,child.children.count*Swift.MemoryLayout<SrlXmlNode>.stride+child.text.utf8.count)
                    result.append(contentsOf:child.children); text += child.text
                }
            }
            node.children = result; node.text = text
        }
        /// Project only selected payloads; unselected descendants do not impose capability requirements.
        private func visit(_ node: SrlXmlNode, _ parent: Scope?, _ selected: Swift.Bool) throws -> Swift.Int {
            try budget.consume(.workUnits,1); let scope = try declarations(node,parent)
            let uri = node.name.namespace, local = node.name.local
            if uri != relationshipNamespace, try ignored(scope,uri) {
                guard try process(scope,node.name) else { return 2 }
                guard !node.attributes.contains(where: { $0.name.namespace == Self.xmlNamespace && ["base","lang","space"].contains($0.name.local) }) else { throw Failure(.invalidMetadata,"XML context on unwrapped element") }
                try must(node); try children(node,scope); return 1
            }
            if uri == Self.namespace {
                if local == "AlternateContent" {
                    _ = try wrapper(node,scope,false); guard srlXmlWhitespace(node.text) else { throw Failure(.invalidMetadata,"Text in AlternateContent") }
                    var choices = 0, fallback = false; var branch: SrlXmlNode?
                    for child in node.children {
                        let childScope = try declarations(child,scope)
                        if child.name.namespace != relationshipNamespace, try ignored(childScope,child.name.namespace) {
                            if try !process(childScope,child.name) { continue }
                        }
                        guard child.name.namespace == Self.namespace else { throw Failure(.invalidMetadata,"Unexpected AlternateContent child") }
                        if child.name.local == "Choice" && !fallback {
                            choices += 1; let understood = try wrapper(child,childScope,true); if branch == nil && understood { branch = child }
                        } else if child.name.local == "Fallback" && choices > 0 && !fallback {
                            fallback = true; _ = try wrapper(child,childScope,false); if branch == nil { branch = child }
                        } else { throw Failure(.invalidMetadata,"Invalid Choice/Fallback order") }
                    }
                    guard choices > 0 else { throw Failure(.invalidMetadata,"AlternateContent has no Choice") }; try must(node)
                    if let branch { _ = try visit(branch,scope,true); node.children = branch.children; node.text = branch.text }
                    else { node.children = []; node.text = "" }
                    return 1
                }
                guard selected && (local == "Choice" || local == "Fallback") else { throw Failure(.invalidMetadata,"Unexpected compatibility element") }
                _ = try wrapper(node,scope,local == "Choice"); try must(node); try children(node,scope); return 1
            }
            try must(node); try children(node,scope)
            var attributes: [SrlXmlAttribute] = []
            for attribute in node.attributes {
                if attribute.name.namespace == Self.namespace { continue }
                if attribute.name.namespace != relationshipNamespace, try ignored(scope,attribute.name.namespace) { continue }
                try budget.consume(.decoderAllocationBytes,Swift.MemoryLayout<SrlXmlAttribute>.stride); attributes.append(attribute)
            }
            node.attributes = attributes; return 0
        }
        /// Require exactly one resulting root, including an AlternateContent document-root wrapper.
        func apply(_ root: SrlXmlNode) throws -> SrlXmlNode {
            try budget.consume(.decoderAllocationBytes,Swift.MemoryLayout<SrlXmlNode>.stride)
            let container = SrlXmlNode(SrlXmlName(local:"",namespace:"")); container.children = [root]
            try children(container,nil)
            guard container.children.count == 1, srlXmlWhitespace(container.text) else { throw Failure(.invalidMetadata,"Compatibility output requires one root") }
            return container.children[0]
        }
    }
}
)packagemce"
