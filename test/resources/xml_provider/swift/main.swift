// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation
import WinSDK

// Keep independent fixture expectations outside expected-failure catches.
func require(_ value: Bool) throws {
    guard value else { throw SerializerError.invalid("Prepared provider expectation failed") }
}

// Fail if an invalid operation returns normally; preserve thrown provider diagnostics.
func rejected(_ action: () throws -> Void) throws {
    do { try action() } catch { try require(!String(describing:error).isEmpty); return }
    throw SerializerError.invalid("Prepared provider accepted invalid input")
}

// Load only a caller-selected local test bridge; prepared handles retain this module's lifetime.
final class ProviderBridge: @unchecked Sendable {
    typealias Prepare = @convention(c) (UnsafePointer<UInt8>?, UInt64) -> UnsafeMutableRawPointer?
    typealias Validate = @convention(c) (UnsafeRawPointer?, UnsafePointer<UInt8>?, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64) -> Int32
    typealias Release = @convention(c) (UnsafeMutableRawPointer?) -> Void
    typealias Message = @convention(c) (UnsafeMutablePointer<UInt8>?, UInt64) -> UInt64
    let library: HMODULE
    let prepare: Prepare
    let validate: Validate
    let release: Release
    let diagnostic: Message
    let version: Message

    // Bind every stable fixture symbol before any schema or document callback is attempted.
    init(_ path: String) throws {
        let wide=Array(path.utf16)+[0]
        guard let module=wide.withUnsafeBufferPointer({ LoadLibraryExW($0.baseAddress,nil,DWORD(LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)) }) else {
            throw SerializerError.invalid("Cannot load prepared provider bridge")
        }
        // Resolve only fixed local ABI symbols; failed initialization releases its module.
        func symbol<T>(_ name: String, _ type: T.Type) throws -> T {
            guard let address=name.withCString({ GetProcAddress(module,$0) }) else {
                throw SerializerError.invalid("Missing prepared provider symbol: "+name)
            }
            return unsafeBitCast(address,to:type)
        }
        do {
            prepare=try symbol("serializer_xml_provider_prepare",Prepare.self)
            validate=try symbol("serializer_xml_provider_validate",Validate.self)
            release=try symbol("serializer_xml_provider_release",Release.self)
            diagnostic=try symbol("serializer_xml_provider_error",Message.self)
            version=try symbol("serializer_xml_provider_version",Message.self)
            library=module
        } catch { _=FreeLibrary(module); throw error }
    }

    // Unload only after every prepared schema retaining this bridge has been released.
    deinit { _=FreeLibrary(library) }

    // Copy the same synchronous OS thread's bounded provider diagnostic before returning.
    func message(_ function: Message) -> String {
        var bytes=[UInt8](repeating:0,count:1024)
        let size=bytes.withUnsafeMutableBufferPointer { function($0.baseAddress,UInt64($0.count)) }
        return String(decoding:bytes.prefix(min(Int(size),bytes.count-1)),as:UTF8.self)
    }
}

// Share one immutable prepared schema while giving every validation a fresh native context.
final class PreparedProvider: XmlValidator, @unchecked Sendable {
    let bridge: ProviderBridge
    private let condition=NSCondition()
    private var handle: UnsafeMutableRawPointer?
    private var readers=0
    private(set) var calls=0

    // Compile the complete borrowed schema synchronously; no external schema acquisition is allowed.
    init(_ bridge: ProviderBridge, _ source: [UInt8]) throws {
        self.bridge=bridge
        handle=source.withUnsafeBufferPointer { bridge.prepare($0.baseAddress,UInt64($0.count)) }
        guard handle != nil else { throw SerializerError.invalid(bridge.message(bridge.diagnostic)) }
    }

    // Release the owning handle once every validation borrowing it has completed.
    func close() {
        condition.lock()
        let previous=handle; handle=nil
        while readers != 0 { condition.wait() }
        condition.unlock()
        if let value=previous { bridge.release(value) }
    }

    // Keep cleanup idempotent for explicitly closed fixtures and failure exits.
    deinit { close() }

    // Apply generated complete-message bounds using the actual prepared provider.
    func validate(_ bytes: [UInt8], _ limits: XmlLimits) throws {
        guard limits.maxBytes >= 0 && limits.maxDepth >= 0 && limits.maxNodes >= 0 && limits.maxTextBytes >= 0 else {
            throw SerializerError.invalid("Negative prepared provider limit")
        }
        try validateBounds(bytes,[limits.maxBytes,limits.maxDepth,limits.maxNodes,limits.maxNodes,limits.maxTextBytes])
    }

    // Admit a borrow under the lifetime lock, then validate concurrently outside it.
    func validateBounds(_ bytes: [UInt8], _ bounds: [Int]) throws {
        condition.lock()
        guard let value=handle else { condition.unlock(); throw SerializerError.invalid("Prepared provider is closed") }
        readers += 1; calls += 1; condition.unlock()
        defer { condition.lock(); readers -= 1; condition.broadcast(); condition.unlock() }
        let status=bytes.withUnsafeBufferPointer {
            bridge.validate(value,$0.baseAddress,UInt64($0.count),UInt64(bounds[0]),UInt64(bounds[1]),UInt64(bounds[2]),UInt64(bounds[3]),UInt64(bounds[4]))
        }
        guard status == 0 else { throw SerializerError.invalid(bridge.message(bridge.diagnostic)) }
    }
}

// Qualify genuine provider callbacks, candidate publication, independent bounds and concurrent reuse.
let bridge=try ProviderBridge(CommandLine.arguments[1])
let provider=try PreparedProvider(bridge,Array(Data(contentsOf:URL(fileURLWithPath:CommandLine.arguments[2]))))
let document="<order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer><value>3</value></order>"
let bytes=Array(document.utf8)
let value=try XmlProviderFixtureOrder.decodeXml(bytes,validator:provider)
try require(value.id == 7 && value.customer == "R&D" && value.values == [3])
let output=try value.encodeXml(validator:provider)
try provider.validate(output,XmlLimits())
let copy=try XmlProviderFixtureOrder.decodeXml(output,validator:provider)
try require(value.encode(.BINARY_NONE) == copy.encode(.BINARY_NONE))
try require(provider.calls >= 4)
for bad in ["<wrong xmlns='urn:serializer:xsd'/>",document.replacingOccurrences(of:"urn:serializer:xsd",with:"urn:wrong"),document+"<extra/>",document.replacingOccurrences(of:"<value>3</value>",with:""),"<!DOCTYPE order SYSTEM 'file:///must-not-open'>"+document,"<?xml version='1.1'?>"+document] {
    try rejected { try provider.validate(Array(bad.utf8),XmlLimits()) }
}
try rejected { try provider.validate([0xc3,0x28],XmlLimits()) }
let mismatch=try PreparedProvider(bridge,Array("<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:serializer:xsd'><xs:element name='order' type='xs:string'/></xs:schema>".utf8))
try rejected { _=try value.encodeXml(validator:mismatch) }
try rejected { _=try XmlProviderFixtureOrder.decodeXml(bytes,validator:mismatch) }
for directive in ["include","import","redefine","override"] {
    for location in ["file:///must-not-open.xsd","https://invalid.example/must-not-open.xsd"] {
        let schema="<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:"+directive+" schemaLocation='"+location+"'/></xs:schema>"
        try rejected { _=try PreparedProvider(bridge,Array(schema.utf8)) }
    }
}
let hinted=document.replacingOccurrences(of:" id=",with:" xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:schemaLocation='urn:serializer:xsd https://invalid.example/must-not-open.xsd' id=")
try provider.validate(Array(hinted.utf8),XmlLimits())
for index in 0..<5 {
    var limits=[16777216,64,100000,100000,8388608]
    limits[index]=index == 0 ? bytes.count-1 : index == 3 ? 0 : 1
    try rejected { try provider.validateBounds(bytes,limits) }
}
// Protect diagnostic collection without serializing the concurrent provider operations.
let failureLock=NSLock(); var failures=[String]()
DispatchQueue.concurrentPerform(iterations:8) { _ in
    do {
        for _ in 0..<16 {
            try provider.validate(bytes,XmlLimits())
            try rejected { try provider.validate(Array("<wrong/>".utf8),XmlLimits()) }
            try provider.validate(bytes,XmlLimits())
        }
    } catch { failureLock.lock(); failures.append(String(describing:error)); failureLock.unlock() }
}
try require(failures.isEmpty)
try require(!bridge.message(bridge.version).isEmpty)
provider.close(); try rejected { try provider.validate(bytes,XmlLimits()) }
print("Swift prepared libxml2 provider: owning input/output, bounds, no-fetch and concurrent reuse pass")
