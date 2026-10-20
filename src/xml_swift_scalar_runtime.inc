// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlscalar(
// Keep resource exhaustion distinct from an ordinary nonmatching union alternative.
fileprivate struct SrlXmlScalarLimit: Error { let message: String; var original: (any Swift.Error)? = nil }
// Share one deterministic allowance across all restrictions on an active scalar.
fileprivate final class SrlXmlScalarWork {
    var remaining = srlXmlMaxPatternWork
    let maximumItems: Int
    private let operation: ((String,Int) throws -> Void)?
    // Bind the operation's native list admission bound without changing document limits.
    init(_ maximumItems: Int, _ operation: ((String,Int) throws -> Void)? = nil) { self.maximumItems = maximumItems; self.operation=operation }
    // Charge before visiting instructions or retaining bounded scalar work.
    func spend(_ count: Int) throws {
        // Union probes must preserve the original caller error rather than treat it as a nonmatching arm.
        do { try operation?("work",count) } catch { throw SrlXmlScalarLimit(message:"",original:error) }
        guard count >= 0 && count <= remaining else { throw SrlXmlScalarLimit(message:"Portable XML regex work limit exceeded") }; remaining -= count
    }
}
// Apply immutable compiler-owned scalar metadata before input conversion or output publication.
fileprivate final class SrlXmlRule {
    let kind: String, whitespace: String, bounds: [String], enumeration: [String], listKind: String
    let length: Int, minimum: Int, maximum: Int
    let finite: Bool
    let groups: [[Pattern]], members: [SrlXmlRule]
    let fixed: String?
    // Retain only compiled metadata; counters remain local to each complete check.
    init(_ kind: String, _ whitespace: String, _ bounds: [String], _ length: Int, _ minimum: Int,
        _ maximum: Int, _ enumeration: [String], _ finite: Bool, _ groups: [[Pattern]],
        _ listKind: String, _ members: [SrlXmlRule], _ fixed: String?) {
        self.kind=kind; self.whitespace=whitespace; self.bounds=bounds; self.length=length
        self.minimum=minimum; self.maximum=maximum; self.enumeration=enumeration; self.finite=finite
        self.groups=groups; self.listKind=listKind; self.members=members; self.fixed=fixed
    }
    // Normalize only XML whitespace, preserving every other Unicode scalar identity.
    private func normalize(_ raw: String) -> String {
        guard whitespace == "replace" || whitespace == "collapse" else { return raw }
        var result=""; var pending=false
        for scalar in raw.unicodeScalars {
            let space=scalar.value == 9 || scalar.value == 10 || scalar.value == 13 || scalar.value == 32
            if whitespace == "collapse" && space { pending = !result.isEmpty; continue }
            if pending { result.append(" "); pending=false }
            result.unicodeScalars.append(space ? Unicode.Scalar(32)! : scalar)
        }
        return result
    }
    // Recognize exact builtin integer families independently of their native storage.
    private func integerKind() -> Bool {
        switch kind {
        case "byte","unsignedByte","short","unsignedShort","int","unsignedInt","long","unsignedLong","integer","positiveInteger","negativeInteger","nonPositiveInteger","nonNegativeInteger": return true
        default: return false
        }
    }
    // Use host regex only for fixed builtin lexical grammars, never schema-authored patterns.
    private func lexicalMatch(_ text: String, _ expression: String) -> Bool {
        text.range(of:"\\A(?:" + expression + ")\\z",options:.regularExpression) != nil
    }
    // Preserve unbounded decimal digits and scale without binary floating-point conversion.
    private func decimal(_ text: String) -> (negative: Bool, digits: [UInt8], scale: Int) {
        let raw=Array(text.utf8); let negative=raw.first == 45
        let start=raw.first == 45 || raw.first == 43 ? 1 : 0
        let dot=raw[start...].firstIndex(of:46); var scale=dot.map { raw.count-$0-1 } ?? 0
        let digits=raw[start...].filter { $0 != 46 }; var first=0; var end=digits.count
        while first < end && digits[first] == 48 { first += 1 }
        while end > first && digits[end-1] == 48 { end -= 1; scale -= 1 }
        return first == end ? (false,[],0) : (negative,Array(digits[first..<end]),scale)
    }
    // Compare exact decimal values using digit lengths followed by zero-padded lexical order.
    private func decimalOrder(_ left: String, _ right: String) -> Int {
        let a=decimal(left), b=decimal(right)
        if a.digits.isEmpty && b.digits.isEmpty { return 0 }
        if a.negative != b.negative { return a.negative ? -1 : 1 }
        var order=0
        if a.digits.isEmpty { order = -1 }
        else if b.digits.isEmpty { order = 1 }
        else if a.digits.count-a.scale != b.digits.count-b.scale { order = a.digits.count-a.scale < b.digits.count-b.scale ? -1 : 1 }
        else {
            for index in 0..<max(a.digits.count,b.digits.count) {
                let first=index < a.digits.count ? a.digits[index] : 48
                let second=index < b.digits.count ? b.digits[index] : 48
                if first != second { order = first < second ? -1 : 1; break }
            }
        }
        return a.negative ? -order : order
    }
    // Validate builtin lexical and value ranges before facets can compare candidates.
    private func lexical(_ text: String) throws {
        if integerKind() {
            guard lexicalMatch(text,"[+-]?[0-9]+") else { throw SerializerError.invalid("Invalid XML integer") }
            let range: (String,String)?
            switch kind {
            case "byte": range=(String(Int8.min),String(Int8.max))
            case "unsignedByte": range=("0",String(UInt8.max))
            case "short": range=(String(Int16.min),String(Int16.max))
            case "unsignedShort": range=("0",String(UInt16.max))
            case "int": range=(String(Int32.min),String(Int32.max))
            case "unsignedInt": range=("0",String(UInt32.max))
            case "long": range=(String(Int64.min),String(Int64.max))
            case "unsignedLong": range=("0",String(UInt64.max))
            default: range=nil
            }
            if let bound=range { guard decimalOrder(text,bound.0) >= 0 && decimalOrder(text,bound.1) <= 0 else { throw SerializerError.invalid("Overflowing XML integer") } }
            let sign=decimalOrder(text,"0")
            guard !(kind == "positiveInteger" && sign <= 0 || kind == "negativeInteger" && sign >= 0 || kind == "nonPositiveInteger" && sign > 0 || kind == "nonNegativeInteger" && sign < 0) else { throw SerializerError.invalid("XML integer sign failed") }
        } else {
            switch kind {
            case "decimal": guard lexicalMatch(text,"[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)") else { throw SerializerError.invalid("Invalid XML decimal") }
            case "boolean": _=try srlXmlBoolean(text)
            case "float": _=try srlXmlFloat32(text)
            case "double": _=try srlXmlFloat(text)
            case "language": guard lexicalMatch(text,"[A-Za-z]{1,8}(?:-[A-Za-z0-9]{1,8})*") else { throw SerializerError.invalid("Invalid XML language") }
            case "Name","NCName","NMTOKEN","ID","IDREF","ENTITY":
                guard !text.isEmpty else { throw SerializerError.invalid("Empty XML name token") }; var first = kind != "NMTOKEN"
                for scalar in text.unicodeScalars {
                    let code=scalar.value; let colon=(kind == "Name" || kind == "NMTOKEN") && code == 58
                    guard srlXmlNameStart(code) || colon || !first && (code == 45 || code == 46 || code == 0xb7 || (48...57).contains(code) || (0x300...0x36f).contains(code) || (0x203f...0x2040).contains(code)) else { throw SerializerError.invalid("Invalid XML name token") }
                    first=false
                }
            default: break
            }
        }
    }
    // Build unrestricted builtin list-item metadata without sharing mutable work counters.
    private static func itemRule(_ kind: String) -> SrlXmlRule { SrlXmlRule(kind,"collapse",["","","",""],-1,-1,-1,[],false,[],"",[],nil) }
    // Materialize defaults for missing attributes and empty elements before lexical conversion.
    static func defaultText(_ raw: String?, _ fallback: String, _ attribute: Bool) -> String { raw == nil || !attribute && raw!.isEmpty ? fallback : raw! }
    // Keep empty/whitespace-bearing native items from silently changing lexical list identities.
    static func listText(_ items: [String], _ extent: Int, _ maximum: Int, _ quota: ((String) -> any Swift.Error)? = nil) throws -> String {
        guard items.count <= maximum else { throw srlXmlLimitFailure(quota,"Unrepresentable XML list items or size") }
        guard items.count <= maximum && (extent == 0 || items.count == extent), items.allSatisfy({ !$0.isEmpty && !$0.unicodeScalars.contains(where: { $0.value == 32 || $0.value == 9 || $0.value == 10 || $0.value == 13 }) }) else { throw SerializerError.invalid("Unrepresentable XML list items or size") }
        return items.joined(separator:" ")
    }
    // Count before allocating item strings, splitting only the four XML whitespace characters.
    static func tokens(_ raw: String, _ maximum: Int = srlXmlMaxListItems, _ quota: ((String) -> any Swift.Error)? = nil) throws -> [String] {
        var count=0; var item=false
        for scalar in raw.unicodeScalars {
            let space=scalar.value == 32 || scalar.value == 9 || scalar.value == 10 || scalar.value == 13
            if !space && !item { guard count < maximum else { if let quota { throw quota("XML list item limit exceeded") }; throw SrlXmlScalarLimit(message:"XML list item limit exceeded") }; count += 1 }; item = !space
        }
        return raw.split(whereSeparator:{ character in character.unicodeScalars.allSatisfy { $0.value == 32 || $0.value == 9 || $0.value == 10 || $0.value == 13 } }).map(String.init)
    }
    // Compare typed value spaces; unordered NaN can only equal another NaN enumeration value.
    private func compare(_ left: String, _ right: String) throws -> Int? {
        if !listKind.isEmpty {
            let a=try Self.tokens(left,Int.max), b=try Self.tokens(right,Int.max)
            guard a.count == b.count else { return 1 }; let item=Self.itemRule(listKind)
            for index in a.indices { if try item.compare(a[index],b[index]) != 0 { return 1 } }; return 0
        }
        if kind == "float" || kind == "double" {
            let a=kind == "float" ? Double(try srlXmlFloat32(left)) : try srlXmlFloat(left)
            let b=kind == "float" ? Double(try srlXmlFloat32(right)) : try srlXmlFloat(right)
            if a.isNaN || b.isNaN { return a.isNaN && b.isNaN ? 0 : nil }; return a < b ? -1 : a > b ? 1 : 0
        }
        if integerKind() || kind == "decimal" { return decimalOrder(left,right) }
        if kind == "boolean" { return try srlXmlBoolean(left) == srlXmlBoolean(right) ? 0 : 1 }
        return left.utf8.elementsEqual(right.utf8) ? 0 : left.utf8.lexicographicallyPrecedes(right.utf8) ? -1 : 1
    }
    // Apply all restrictions using the operation's finite list policy and fresh work allowance.
    func check(_ raw: String, _ maximumItems: Int = srlXmlMaxListItems, _ quota: ((String) -> any Swift.Error)? = nil, _ operation: ((String,Int) throws -> Void)? = nil) throws -> String {
        do { return try check(raw,SrlXmlScalarWork(maximumItems,operation)) }
        catch let error as SrlXmlScalarLimit { if let original=error.original { throw original }; throw srlXmlLimitFailure(quota,error.message) }
    }
    // Preserve work exhaustion across union probing and inherited restriction groups.
    private func check(_ raw: String, _ work: SrlXmlScalarWork) throws -> String {
        try work.spend(raw.utf8.count); let text=normalize(raw); try lexical(text)
        if !members.isEmpty {
            var matched=false
            for member in members { do { _=try member.check(text,work); matched=true; break } catch let error as SrlXmlScalarLimit { throw error } catch { } }
            guard matched else { throw SerializerError.invalid("XML union has no matching member") }
        }
        var count=text.unicodeScalars.count
        if !listKind.isEmpty { let items=try Self.tokens(text,work.maximumItems); count=items.count; let rule=Self.itemRule(listKind); for item in items { _=try rule.check(item,work) } }
        guard (length < 0 || count == length) && (minimum < 0 || count >= minimum) && (maximum < 0 || count <= maximum) else { throw SerializerError.invalid("XML length facet failed") }
        for index in bounds.indices where !bounds[index].isEmpty {
            guard let order=try compare(text,bounds[index]), index == 0 ? order >= 0 : index == 1 ? order <= 0 : index == 2 ? order > 0 : order < 0 else { throw SerializerError.invalid("XML numeric facet failed") }
        }
        if !enumeration.isEmpty { var matched=false; for value in enumeration { if try compare(text,normalize(value)) == 0 { matched=true; break } }; guard matched else { throw SerializerError.invalid("XML enumeration facet failed") } }
        if let fixed=fixed { guard try compare(text,normalize(fixed)) == 0 else { throw SerializerError.invalid("XML fixed value differs") } }
        if finite { let value=kind == "float" ? Double(try srlXmlFloat32(text)) : try srlXmlFloat(text); guard value.isFinite else { throw SerializerError.invalid("XML finite facet failed") } }
        for group in groups { var matched=false; for pattern in group { if try pattern.matches(text,work) { matched=true; break } }; guard matched else { throw SerializerError.invalid("XML portable pattern facet failed") } }
        return text
    }
    // Avoid activating a nonempty default while preserving normalized empty authored values.
    func output(_ raw: String, _ fallback: String, _ maximumItems: Int = srlXmlMaxListItems, _ quota: ((String) -> any Swift.Error)? = nil, _ operation: ((String,Int) throws -> Void)? = nil) throws -> String {
        let text=try check(raw,maximumItems,quota,operation)
        if text.isEmpty && !normalize(fallback).isEmpty { guard whitespace == "collapse" else { throw SerializerError.invalid("Empty XML value would activate a different XSD default") }; return " " }; return text
    }
)xmlscalar"
R"xmlscalar(
    // Execute compiler-lowered Thompson instructions without host-regex semantics or backtracking.
    final class Pattern {
        private struct State { let epsilon: [Int], ranges: [Int], next: Int, flags: Int }
        private let start: Int, states: [State]
        // Own validated compiler instructions; no untrusted text reaches this constructor.
        private init(_ start: Int, _ states: [State]) { self.start=start; self.states=states }
        // Decode bounded compiler-owned fragments once, preserving exact Unicode range values.
        static func prepare(_ fragments: [String]) -> Pattern {
            let records=fragments.joined().split(separator:";",omittingEmptySubsequences:false); let count=records.count-1
            precondition(count > 0 && count <= srlXmlMaxPatternStates); let start=Int(records[0])!; precondition(start >= 0 && start < count)
            // Parse one immutable instruction array; its producer is the checked native compiler.
            func integers(_ text: Substring) -> [Int] { text.isEmpty ? [] : text.split(separator:",").map { Int($0)! } }
            let states=(1...count).map { index -> State in
                let fields=records[index].split(separator:"|",omittingEmptySubsequences:false); precondition(fields.count == 4)
                let epsilon=integers(fields[0]), ranges=integers(fields[1]), next=Int(fields[2])!, flags=Int(fields[3])!
                precondition(ranges.count % 2 == 0 && next >= 0 && next < count && flags >= 0 && flags <= 15 && epsilon.allSatisfy { $0 >= 0 && $0 < count })
                for range in stride(from:0,to:ranges.count,by:2) { precondition(ranges[range] >= 0 && ranges[range] <= ranges[range+1] && ranges[range+1] <= 0x10ffff) }
                return State(epsilon:epsilon,ranges:ranges,next:next,flags:flags)
            }
            return Pattern(start,states)
        }
        // Mark on insertion so epsilon cycles fit a fixed pending-state array.
        private func closure(_ selected: inout [Int], _ count: Int, _ visited: inout [Bool], _ stack: inout [Int], _ work: SrlXmlScalarWork) throws -> Int {
            try work.spend(visited.count); for index in visited.indices { visited[index]=false }; var pending=0; var kept=0
            for index in 0..<count { let target=selected[index]; try work.spend(1); if !visited[target] { visited[target]=true; stack[pending]=target; pending += 1 } }
            while pending != 0 {
                try work.spend(1); pending -= 1; let current=stack[pending]; let state=states[current]
                if state.flags & 9 != 0 { selected[kept]=current; kept += 1 }
                for target in state.epsilon { try work.spend(1); if !visited[target] { visited[target]=true; stack[pending]=target; pending += 1 } }
            }
            return kept
        }
        // Match complete scalar sequences with fixed state arrays and bounded instruction work.
        func matches(_ text: String, _ work: SrlXmlScalarWork) throws -> Bool {
            guard text.utf8.count <= srlXmlMaxPatternBytes else { throw SrlXmlScalarLimit(message:"Portable XML regex input limit exceeded") }
            var visited=[Bool](repeating:false,count:states.count), active=[Int](repeating:0,count:states.count)
            var next=active, stack=active; active[0]=start; var count=try closure(&active,1,&visited,&stack,work)
            for scalar in text.unicodeScalars {
                try work.spend(1); let code=Int(scalar.value); var following=0
                for index in 0..<count {
                    try work.spend(1); let state=states[active[index]]; if state.flags & 1 == 0 { continue }; var matched=false
                    if state.flags & 4 != 0 { matched=code != 10 && code != 13 }
                    else { for range in stride(from:0,to:state.ranges.count,by:2) { try work.spend(1); matched = matched || code >= state.ranges[range] && code <= state.ranges[range+1] }; if state.flags & 2 != 0 { matched = !matched } }
                    if matched { next[following]=state.next; following += 1 }
                }
                count=try closure(&next,following,&visited,&stack,work); swap(&active,&next)
            }
            for index in 0..<count { if states[active[index]].flags & 8 != 0 { return true } }; return false
        }
    }
}
// Convert a complete lexical list into private native values before assigning the owner field.
fileprivate func srlXmlReadList<T>(_ text: String, _ extent: Int, _ maximum: Int, _ quota: ((String) -> any Swift.Error)? = nil,
    _ decode: (String) throws -> T) throws -> [T] {
    let items=try SrlXmlRule.tokens(text,maximum,quota)
    guard extent == 0 || items.count == extent else { throw SerializerError.invalid("XML lexical list fixed extent") }
    return try items.map(decode)
}
)xmlscalar"
