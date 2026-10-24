// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"swifthtml(
// Bound approved ordinary HTML independently of XML and native wire policy.
public struct HtmlLimits {
    public var maxInputBytes: Int
    public var maxOutputBytes: Int
    public var maxEvents: Int
    public var maxDepth: Int
    public var maxAttributesPerElement: Int
    public var maxTextBytes: Int
    public init(maxInputBytes: Int = 8388608, maxOutputBytes: Int = 16777216,
                maxEvents: Int = 200000, maxDepth: Int = 128,
                maxAttributesPerElement: Int = 256, maxTextBytes: Int = 8388608) {
        self.maxInputBytes=maxInputBytes; self.maxOutputBytes=maxOutputBytes
        self.maxEvents=maxEvents; self.maxDepth=maxDepth
        self.maxAttributesPerElement=maxAttributesPerElement; self.maxTextBytes=maxTextBytes
    }
    // Reject edited invalid limits at every public operation boundary.
    fileprivate func validate() throws {
        guard [maxInputBytes,maxOutputBytes,maxEvents,maxDepth,maxAttributesPerElement,maxTextBytes].allSatisfy({ $0 >= 0 }) else {
            throw SerializerError.invalid("Invalid HTML limit")
        }
    }
}
// Own one lowercase approved attribute; hidden uses Boolean presence without an assigned value.
public struct HtmlAttribute {
    public var name: String
    public var value: String
    public var boolean: Bool
    public init(_ name: String, value: String = "", boolean: Bool = false) {
        self.name=name; self.value=value; self.boolean=boolean
    }
}
// Own semantic fragment/start/text/end events without XML namespaces or xsi metadata.
public struct HtmlEvent {
    public var kind: String
    public var name: String
    public var attributes: [HtmlAttribute]
    public var text: String
    public init(_ kind: String, name: String = "", attributes: [HtmlAttribute] = [], text: String = "") {
        self.kind=kind; self.name=name; self.attributes=attributes; self.text=text
    }
}

// Parse and write explicitly balanced approved HTML; browser recovery and URL sanitization are outside this contract.
public enum Html {
    private static let tags=Set("div p span b i strong em a ul ol li section article h1 h2 h3 h4 h5 h6 br hr img small s u code blockquote nav sub sup".split(separator:" ").map(String.init))
    private static let voids: Set<String>=["br","hr","img"]
    private static let phrasing=Set("span b i strong em a br img small s u code sub sup".split(separator:" ").map(String.init))
    private static let headings: Set<String>=["h1","h2","h3","h4","h5","h6"]
    private static let ordinaryAttributes: Set<String>=["id","class","title","lang","dir","hidden"]
    private static let entities=["amp":"&","lt":"<","gt":">","quot":"\"","apos":"'","nbsp":"\u{a0}"]
    // Describe the precise subset without claiming general HTML5 parsing or sanitization.
    public static func supportedSubset() -> String {
        "UTF-8 HTML fragments; explicit balanced approved flow/phrasing/list tags; br/hr/img void tags; ASCII case-insensitive names; quoted/unquoted and hidden Boolean attributes; amp/lt/gt/quot/apos/nbsp and numeric references; comments omitted; no doctype, implied closes, raw text, foreign content or browser recovery"
    }
    // Validate semantic Unicode scalars, including HTML form-feed whitespace and excluding unsupported controls.
    private static func text(_ value: String) throws {
        for item in value.unicodeScalars {
            let code=item.value
            guard code != 0, !(0x7f...0x9f).contains(code), code >= 0x20 || [9,10,12,13].contains(code) else {
                throw SerializerError.invalid("Unsupported HTML control character")
            }
        }
    }
    // Decode approved terminated references and normalize source CR/CRLF while retaining referenced CR scalars.
    private static func decode(_ input: String) throws -> String {
        let normalized=input.replacingOccurrences(of:"\r\n",with:"\n").replacingOccurrences(of:"\r",with:"\n")
        let source=Array(normalized.utf8); var result=""; var position=0
        while position < source.count {
            guard let found=source[position...].firstIndex(of:38) else {
                result += String(decoding:source[position...],as:UTF8.self); break
            }
            result += String(decoding:source[position..<found],as:UTF8.self)
            guard let end=source[(found+1)...].firstIndex(of:59), end-found <= 12 else {
                throw SerializerError.invalid("HTML references require supported terminated spelling")
            }
            let entity=String(decoding:source[(found+1)..<end],as:UTF8.self)
            let decoded: String
            if let known=entities[entity] { decoded=known }
            else {
                let bytes=Array(entity.utf8)
                guard bytes.first == 35 else { throw SerializerError.invalid("HTML entity outside approved subset") }
                let hex=bytes.count > 1 && (bytes[1] == 120 || bytes[1] == 88)
                let digits=bytes.dropFirst(hex ? 2 : 1)
                guard !digits.isEmpty, digits.allSatisfy({ byte in
                    (48...57).contains(byte) || hex && ((65...70).contains(byte) || (97...102).contains(byte))
                }), let code=UInt32(String(decoding:digits,as:UTF8.self),radix:hex ? 16 : 10),
                    let scalar=Unicode.Scalar(code) else { throw SerializerError.invalid("Malformed HTML numeric reference") }
                decoded=String(scalar)
            }
            try text(decoded); result += decoded; position=end+1
        }
        try text(result); return result
    }
    // Refuse relationships that independent HTML parsers would implicitly close or rearrange.
    private static func child(_ open: [String], _ child: String) throws {
        guard tags.contains(child) else { throw SerializerError.invalid("HTML tag outside approved subset") }
        let parent=open.last
        guard child != "li" || parent == "ul" || parent == "ol" else { throw SerializerError.invalid("HTML list item requires direct list parent") }
        if let parent {
            guard !voids.contains(parent), (parent != "ul" && parent != "ol") || child == "li",
                (!phrasing.contains(parent) && parent != "p" && !headings.contains(parent)) || phrasing.contains(child) else {
                throw SerializerError.invalid("HTML child requires unsupported browser recovery")
            }
        }
        guard child != "a" || !open.contains("a") else { throw SerializerError.invalid("Nested HTML anchor outside approved subset") }
    }
    // Validate an owning trace completely before returning deterministic ordinary HTML UTF-8 bytes.
    public static func writeFragment(_ events: [HtmlEvent], limits: HtmlLimits = HtmlLimits()) throws -> [UInt8] {
        try limits.validate()
        guard events.count >= 2, events.count <= limits.maxEvents,
            events.first?.kind == "start_fragment", events.last?.kind == "end_fragment" else {
            throw SerializerError.invalid("HTML requires bounded explicit fragment boundaries")
        }
        var parts: [String]=[]; var open: [String]=[]; var bytes=0; var textBytes=0
        // Admit each bounded escaped batch before retaining it in the private result.
        func append(_ value: String) throws {
            let count=value.utf8.count
            guard count <= limits.maxOutputBytes-bytes else { throw SerializerError.invalid("HTML output byte limit") }
            bytes += count; parts.append(value)
        }
        // Escape semantic text without interpreting URLs or applying XML character restrictions.
        func escape(_ value: String, attribute: Bool = false) throws {
            try text(value); var batch=""
            for item in value.unicodeScalars {
                switch item.value {
                case 38: batch += "&amp;"
                case 60: batch += "&lt;"
                case 62: batch += "&gt;"
                case 13: batch += "&#13;"
                case 34: batch += attribute ? "&quot;" : "\""
                default: batch.unicodeScalars.append(item)
                }
                if batch.utf8.count >= 4096 { try append(batch); batch="" }
            }
            if !batch.isEmpty { try append(batch) }
        }
        for (index,event) in events.enumerated() {
            switch event.kind {
            case "start_fragment","end_fragment":
                guard index == 0 || index == events.count-1, open.isEmpty,
                    event.name.isEmpty, event.attributes.isEmpty, event.text.isEmpty else { throw SerializerError.invalid("Malformed HTML fragment boundary") }
            case "start_element":
                try child(open,event.name)
                guard event.text.isEmpty, open.count < limits.maxDepth, event.attributes.count <= limits.maxAttributesPerElement else { throw SerializerError.invalid("HTML element payload/depth/attribute limit") }
                try append("<"+event.name); var names=Set<String>()
                for attribute in event.attributes {
                    let key=attribute.name
                    let allowed=ordinaryAttributes.contains(key) || event.name == "a" && ["href","rel"].contains(key) ||
                        event.name == "img" && ["src","alt","width","height"].contains(key)
                    guard allowed, names.insert(key).inserted, attribute.boolean == (key == "hidden"),
                        !attribute.boolean || attribute.value.isEmpty else { throw SerializerError.invalid("Unsupported or duplicate HTML attribute") }
                    try text(attribute.value); try append(" "+key)
                    if !attribute.boolean { try append("=\""); try escape(attribute.value,attribute:true); try append("\"") }
                }
                try append(">"); open.append(event.name)
            case "end_element":
                guard open.last == event.name, event.attributes.isEmpty, event.text.isEmpty else { throw SerializerError.invalid("Unbalanced HTML close") }
                if !voids.contains(event.name) { try append("</"+event.name+">") }; open.removeLast()
            case "text":
                let count=event.text.utf8.count
                guard event.name.isEmpty, event.attributes.isEmpty, !voids.contains(open.last ?? ""),
                    count <= limits.maxTextBytes-textBytes else { throw SerializerError.invalid("Invalid HTML text or cumulative byte limit") }
                if open.last == "ul" || open.last == "ol" {
                    guard event.text.utf8.allSatisfy({ [9,10,12,13,32].contains($0) }) else { throw SerializerError.invalid("HTML lists require whitespace between items") }
                }
                textBytes += count; try escape(event.text)
            default: throw SerializerError.invalid("Unknown HTML event kind")
            }
        }
        return Array(parts.joined().utf8)
    }

)swifthtml"
R"swifthtml(
    // Parse fatal UTF-8 with explicit nesting and publish the event trace only after complete validated EOF.
    public static func parseFragment(_ input: [UInt8], limits: HtmlLimits = HtmlLimits()) throws -> [HtmlEvent] {
        try limits.validate()
        guard input.count <= limits.maxInputBytes, let validated=String(bytes:input,encoding:.utf8) else { throw SerializerError.invalid("HTML input byte limit or invalid UTF-8") }
        try text(validated)
        let source=input; var events: [HtmlEvent]=[]; var open: [String]=[]; var position=0; var textBytes=0
        // Check event and semantic text bounds before retaining a new owning event.
        func add(_ event: HtmlEvent) throws {
            guard events.count < limits.maxEvents else { throw SerializerError.invalid("HTML event count limit") }
            if event.kind == "text" { let count=event.text.utf8.count; guard count <= limits.maxTextBytes-textBytes else { throw SerializerError.invalid("HTML text byte limit") }; textBytes += count }
            events.append(event)
        }
        // The zero sentinel cannot occur in validated HTML and makes EOF checks explicit.
        func at(_ offset: Int) -> UInt8 { offset < source.count ? source[offset] : 0 }
        // Recognize only the HTML ASCII whitespace set.
        func space(_ offset: Int) -> Int { var end=offset; while end < source.count && [9,10,12,13,32].contains(source[end]) { end += 1 }; return end }
        // Read an ASCII syntax name without allocating copies of the unconsumed document.
        func name(_ offset: Int) throws -> (String,Int) {
            var end=offset
            while end < source.count && ((65...90).contains(source[end]) || (97...122).contains(source[end]) || (48...57).contains(source[end]) || source[end] == 45) { end += 1 }
            guard end > offset else { throw SerializerError.invalid("Unsupported HTML tag/attribute syntax") }
            return (String(decoding:source[offset..<end],as:UTF8.self).lowercased(),end)
        }
        // Compare a small ASCII delimiter with bounds checked before indexing.
        func has(_ pattern: [UInt8], _ offset: Int) -> Bool {
            offset <= source.count && pattern.count <= source.count-offset && source[offset..<(offset+pattern.count)].elementsEqual(pattern)
        }
        try add(HtmlEvent("start_fragment"))
        while position < source.count {
            if source[position] != 60 {
                let end=source[position...].firstIndex(of:60) ?? source.count
                try add(HtmlEvent("text",text:decode(String(decoding:source[position..<end],as:UTF8.self)))); position=end; continue
            }
            if has([60,33,45,45],position) {
                var end=position+4; while end < source.count && !has([45,45,62],end) { end += 1 }
                guard end < source.count else { throw SerializerError.invalid("Malformed HTML comment") }
                var check=position+4
                while check < end { guard !has([45,45],check) else { throw SerializerError.invalid("Malformed HTML comment") }; check += 1 }
                position=end+3; continue
            }
            position += 1; let closing=at(position) == 47; if closing { position += 1 }
            let (tag,next)=try name(position); position=next
            if closing {
                position=space(position)
                guard at(position) == 62, !voids.contains(tag), open.last == tag else { throw SerializerError.invalid("HTML close requires matching explicit nonvoid start") }
                position += 1; open.removeLast(); try add(HtmlEvent("end_element",name:tag)); continue
            }
            try child(open,tag); guard open.count < limits.maxDepth else { throw SerializerError.invalid("HTML nesting limit") }
            var attributes: [HtmlAttribute]=[]; var selfClosing=false
            while true {
                let separator=position; position=space(position)
                guard position < source.count else { throw SerializerError.invalid("Truncated HTML start tag") }
                if source[position] == 62 { position += 1; break }
                if has([47,62],position) { selfClosing=true; position += 2; break }
                guard separator != position, attributes.count < limits.maxAttributesPerElement else { throw SerializerError.invalid("HTML attributes require separation and bounded cardinality") }
                let (key,endName)=try name(position); position=endName
                let keyEnd=position; position=space(position); let assigned=at(position) == 61; var value=""
                if assigned {
                    position=space(position+1); guard position < source.count else { throw SerializerError.invalid("Truncated HTML attribute") }
                    let quote: UInt8? = at(position) == 34 || at(position) == 39 ? at(position) : nil
                    if quote != nil { position += 1 }
                    var end=position
                    if let quote {
                        guard let found=source[position...].firstIndex(of:quote) else { throw SerializerError.invalid("Unterminated HTML attribute") }; end=found
                    } else {
                        while end < source.count && ![9,10,12,13,32,62].contains(source[end]) {
                            guard ![34,39,60,61,96].contains(source[end]) else { throw SerializerError.invalid("Invalid unquoted HTML attribute") }; end += 1
                        }
                        guard end > position else { throw SerializerError.invalid("Empty unquoted HTML attribute") }
                    }
                    value=try decode(String(decoding:source[position..<end],as:UTF8.self)); position=end+(quote == nil ? 0 : 1)
                } else { position=keyEnd }
                if key == "hidden" { guard !assigned || value.isEmpty || value.lowercased() == "hidden" else { throw SerializerError.invalid("HTML hidden requires Boolean spelling") }; value="" }
                else { guard assigned else { throw SerializerError.invalid("Only approved Boolean attributes may omit a value") } }
                attributes.append(HtmlAttribute(key,value:value,boolean:key == "hidden"))
            }
            guard !selfClosing || voids.contains(tag) else { throw SerializerError.invalid("HTML self-closing nonvoid tag requires browser recovery") }
            try add(HtmlEvent("start_element",name:tag,attributes:attributes))
            if voids.contains(tag) { try add(HtmlEvent("end_element",name:tag)) } else { open.append(tag) }
        }
        guard open.isEmpty else { throw SerializerError.invalid("HTML fragment has unclosed elements") }; try add(HtmlEvent("end_fragment"))
        _=try writeFragment(events,limits:limits); return events
    }
}
)swifthtml"
