// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation
typealias C = __COMPRESSION_NAME__
/// Keep assertions active in optimized actual SDK builds.
func require(_ value: Bool, _ message: String) { if !value { fatalError(message) } }
/// Retain application failure identity through all provider and wrapper cleanup.
final class Marker: Swift.Error { let message: String; init(_ message: String) { self.message = message } }
/// Require a stable generated compression category independently of diagnostic text.
func reject(_ code: C.Code, _ action: () throws -> Void) {
    do { try action() }
    catch let failure as C.Failure { require(failure.code == code, "Wrong category: \(failure.message)"); return }
    catch { fatalError("Unrelated failure: \(error)") }
    fatalError("Expected rejection: \(code)")
}
/// Check the exact application object survives native/context cleanup.
func same<Original: AnyObject & Swift.Error>(_ original: Original, _ action: () throws -> Void) {
    do { try action() } catch { require(error as AnyObject === original, "Original failure replaced"); return }
    fatalError("Expected original failure")
}
/// Produce deterministic incompressible transport data without nondeterministic SDK fixtures.
func noise(_ count: Int) -> [UInt8] {
    var state: UInt32 = 0x53455249; var result: [UInt8] = []
    for _ in 0..<count { state = state &* 1664525 &+ 1013904223; result.append(UInt8(truncatingIfNeeded: state >> 24)) }
    return result
}
/// Replay independently produced frames and exact input/output/window policies.
func fixtures(_ directory: String, _ name: String, _ format: C.Format, _ provider: C.Provider) throws -> Int {
    let rows = try String(contentsOfFile: directory+"/"+name+".tsv",encoding:.utf8).split(whereSeparator:{$0.isNewline})
    for row in rows {
        let columns = row.split(separator:"\t").map(String.init)
        let packed = Array(try Data(contentsOf:URL(fileURLWithPath:directory+"/"+columns[0])))
        let options = C.DecodeOptions(format:format,provider:provider,maxInputBytes:Int(columns[3])!,maxOutputBytes:Int(columns[4])!,maxWindowBytes:Int(columns[5])!)
        if columns[2] == "ok" {
            let expected = Array(try Data(contentsOf:URL(fileURLWithPath:directory+"/"+columns[1])))
            require(try C.decompress(packed,options:options) == expected,columns[0])
        } else { reject(columns[2] == "RESOURCE_LIMIT" ? .resourceLimit : .invalidData) { _ = try C.decompress(packed,options:options) } }
    }
    return rows.count
}
/// Preserve all four ordinary generated protocol bytes through owning callback adapters.
func models(_ output: String, _ name: String, _ format: C.Format, _ provider: C.Provider) throws {
    var value = AccountRecord(); value.accountId = 42; value.label = "owning value"
    for proto in Protocol.allCases {
        let native = try value.encode(proto)
        try Data(native).write(to:URL(fileURLWithPath:output+"/native-\(proto).bin"))
        let packed = try C.encode({ try value.encode(proto) },options:.init(format:format,provider:provider))
        let candidate = try C.decode(packed,decoder:{ try AccountRecord.decode($0,proto) },options:.init(format:format,provider:provider))
        require(try candidate.encode(proto) == native,"Native protocol changed")
        try Data(packed).write(to:URL(fileURLWithPath:output+"/output-"+name+"-\(proto).bin"))
        do {
            _ = try C.decode(C.compress(native+[0],options:.init(format:format,provider:provider)),decoder:{try AccountRecord.decode($0,proto)},options:.init(format:format,provider:provider))
            fatalError("Accepted native unread suffix")
        } catch is SerializerError { }
        var armed = false; let original = Marker("late candidate cancellation")
        same(original) {
            _ = try C.decode(packed,decoder:{ data in let result = try AccountRecord.decode(data,proto); armed = true; return result },
                options:.init(format:format,provider:provider,checkCancelled:{if armed {throw original}}))
        }
    }
    for level in [0,1,6,9] {
        for sample in [0,1] {
            let plain = sample == 0 ? [] : [UInt8](repeating:65,count:65537)
            let packed = try C.compress(plain,options:.init(format:format,provider:provider,level:level))
            require(try C.decompress(packed,options:.init(format:format,provider:provider)) == plain,"Roundtrip")
            try Data(packed).write(to:URL(fileURLWithPath:output+"/level-"+name+"-\(level)-\(sample).bin"))
        }
    }
}
/// Exercise bounded short borrowed I/O, owned snapshots, exact EOF and sticky transport failures.
func wrappers(_ format: C.Format, _ provider: C.Provider) throws {
    let plain = noise(65537); var mutable = plain; var packed: [UInt8] = []; var calls = 0
    let writer = try C.Output(sink:{ chunk in require(chunk.count <= 32768,"Oversized sink batch");calls += 1;packed += chunk },options:.init(format:format,provider:provider))
    try writer.write(mutable);mutable[0] ^= 1;try writer.flush();require(calls == 0,"Flush published bytes")
    try writer.finish();require(calls >= 2,"Missing bounded writes");writer.close()
    require(try C.decompress(packed,options:.init(format:format,provider:provider)) == plain,"Staged storage alias")
    var position = 0;var requests = 0
    let reader = try C.Input(source:{ maximum in
        requests += 1;require(maximum <= 32768,"Oversized source batch")
        if position == packed.count {return nil}
        let count = min(maximum,min(7,packed.count-position));let result = Array(packed[position..<(position+count)]);position += count;return result
    },options:.init(format:format,provider:provider,maxInputBytes:packed.count))
    var result:[UInt8] = [];while let chunk = try reader.read(19) { result += chunk };try reader.finish();reader.close()
    require(result == plain && position == packed.count && requests > 1,"Borrowed exact EOF changed")
    let empty = try C.Input(source:{_ in []},options:.init(format:format,provider:provider));defer{empty.close()}
    reject(.invalidData) { _ = try empty.read() }
    let original = Marker("borrowed sink failure");var accepted = 0
    let failed = try C.Output(sink:{chunk in if accepted != 0 {throw original};accepted += chunk.count},options:.init(format:format,provider:provider));defer{failed.close()}
    try failed.write(plain);same(original){try failed.finish()};require(accepted > 0,"Missing partial prefix control")
    do {try failed.flush();fatalError("Failed wrapper resumed")} catch let failure as C.Failure {require(failure.cause as AnyObject? === original,"Sticky failure cause changed")}
    let abandoned = try C.Output(sink:{_ in fatalError("Close published bytes")},options:.init(format:format,provider:provider));try abandoned.write([1]);abandoned.close()
}
/// Cancel at every observed owning provider boundary and preserve the original exception object.
func cancellation(_ format: C.Format, _ provider: C.Provider) throws -> Int {
    let plain = [UInt8](repeating:65,count:65537);let packed = try C.compress(plain,options:.init(format:format,provider:provider));var total = 0
    for encoding in [false,true] {
        /// Execute one complete provider operation with an independently selected policy hook.
        func operation(_ check:@escaping () throws -> Void) throws {
            if encoding {_ = try C.compress(plain,options:.init(format:format,provider:provider,checkCancelled:check))}
            else {_ = try C.decompress(packed,options:.init(format:format,provider:provider,checkCancelled:check))}
        }
        var observed = 0;try operation{observed += 1}
        for boundary in 1...observed {
            var count = 0;let original = Marker("optional cancellation")
            same(original){try operation{count += 1;if count == boundary {throw original}}};total += 1
        }
    }
    return total
}
/// Close at every wrapper policy callback and prohibit any later borrowed side effect.
func closeSweep(_ format:C.Format,_ provider:C.Provider) throws -> Int {
    var total = 0
    for reading in [false,true] {
        /// Return the observed hook count for one baseline or selected close boundary.
        func run(_ boundary:Int) throws -> Int {
            let plain = [UInt8](repeating:65,count:100)
            let packed = try C.compress(plain,options:.init(format:format,provider:provider))
            var writer:C.Output?;var reader:C.Input?;var seen = 0;var enabled = reading;var effects = 0;var closedEffects:Int?
            /// Close the active wrapper synchronously while its owning operation is suspended in policy.
            func check() {if enabled {seen += 1;if seen == boundary {writer?.close();reader?.close();closedEffects = effects}}}
            if reading {
                var position = 0
                reader = try C.Input(source:{maximum in effects += 1;if position == packed.count {return nil};let count = min(7,min(maximum,packed.count-position));defer{position += count};return Array(packed[position..<(position+count)])},options:.init(format:format,provider:provider,checkCancelled:check))
            } else {
                writer = try C.Output(sink:{_ in effects += 1},options:.init(format:format,provider:provider,checkCancelled:check))
                try writer!.write(plain);enabled = true
            }
            defer{writer?.close();reader?.close();writer = nil;reader = nil}
            if boundary == 0 {if reading {_ = try reader!.read()} else {try writer!.finish()}}
            else {reject(.invalidOptions){if reading {_ = try reader!.read()} else {try writer!.finish()}};require(closedEffects == effects,"Borrowed call after close")}
            return seen
        }
        let observed = try run(0)
        for boundary in 1...observed {_ = try run(boundary);total += 1}
    }
    return total
}
/// Invalid or reused provider contexts fail before policy can mask their first observed error.
func providers() throws {
    for kind in 0..<6 {
        var returned = false;var closes = 0
        let provider = C.Provider(format:.zstd,minimumLevel:0,maximumLevel:3,defaultLevel:3,
            makeEncoder:{_,_ in C.Context(step:{input,output,_ in
                returned = true
                if kind == 0 {return C.Progress(consumed:-1,produced:0,finished:false)}
                if kind == 1 {return C.Progress(consumed:input.count+1,produced:0,finished:false)}
                if kind == 2 {return C.Progress(consumed:0,produced:32769,finished:false)}
                if kind == 3 {return C.Progress(consumed:0,produced:0,finished:false)}
                if kind == 4 {output = [];return C.Progress(consumed:input.count,produced:0,finished:true)}
                return C.Progress(consumed:0,produced:0,finished:true)
            },close:{closes += 1})},makeDecoder:{_ in fatalError("Unexpected decoder")})
        let original = Marker("Later cancellation must not mask invalid progress")
        reject(.invalidData){_ = try C.compress([1],options:.init(format:.zstd,provider:provider,checkCancelled:{if returned {throw original}}))}
        require(closes == 1,"Provider cleanup count changed")
    }
    let original = Marker("factory failure")
    let broken = C.Provider(format:.zstd,minimumLevel:0,maximumLevel:3,defaultLevel:3,makeEncoder:{_,_ in throw original},makeDecoder:{_ in throw original})
    same(original){_ = try C.compress([1],options:.init(format:.zstd,provider:broken))}
    let nativeFailure = C.NativeProviderFailure(-4)
    let nativeBroken = C.Provider(format:.zstd,minimumLevel:0,maximumLevel:3,defaultLevel:3,
        makeEncoder:{_,_ in C.Context(step:{_,_,_ in throw nativeFailure},close:{})},
        makeDecoder:{_ in C.Context(step:{_,_,_ in throw nativeFailure},close:{})})
    same(nativeFailure){_ = try C.compress([1],options:.init(format:.zstd,provider:nativeBroken))}
    var calls = 0
    reject(.unavailable){_ = try C.encode({calls += 1;return []},options:.init(format:.zstd,provider:C.nativeProvider(.lz4),checkCancelled:{calls += 1}))}
    require(calls == 0,"Provider mismatch invoked application")
    let frame = try C.compress([1],options:.init(format:.zstd,provider:C.nativeProvider(.zstd)))
    var cleanups = 0
    let cached = C.Context(step:{input,output,_ in output.replaceSubrange(0..<frame.count,with:frame);return C.Progress(consumed:input.count,produced:frame.count,finished:true)},close:{cleanups += 1})
    let reused = C.Provider(format:.zstd,minimumLevel:0,maximumLevel:3,defaultLevel:3,makeEncoder:{_,_ in cached},makeDecoder:{_ in fatalError("Unexpected decode")})
    _ = try C.compress([1],options:.init(format:.zstd,provider:reused))
    reject(.invalidOptions){_ = try C.compress([1],options:.init(format:.zstd,provider:reused))};require(cleanups == 1,"Reused context double close")
    var nested:C.Provider?;var nestedCloses = 0;var nestedActive = false;var closedDuringActive = false
    let context = C.Context(step:{input,output,_ in
        nestedActive = true;defer{nestedActive = false}
        reject(.invalidOptions){_ = try C.compress([1],options:.init(format:.zstd,provider:nested!))}
        output.replaceSubrange(0..<frame.count,with:frame)
        return C.Progress(consumed:input.count,produced:frame.count,finished:true)
    },close:{closedDuringActive = nestedActive;nestedCloses += 1})
    nested = C.Provider(format:.zstd,minimumLevel:0,maximumLevel:3,defaultLevel:3,makeEncoder:{_,_ in context},makeDecoder:{_ in fatalError("Unexpected decode")})
    reject(.invalidOptions){_ = try C.compress([1],options:.init(format:.zstd,provider:nested!))}
    require(nestedCloses == 1 && !closedDuringActive,"Reentrant cleanup ran during active provider step");nested = nil
    let zstd = C.nativeProvider(.zstd)
    for level in [zstd.minimumLevel,zstd.maximumLevel] {_ = try C.compress([1],options:.init(format:.zstd,provider:zstd,level:level))}
    _ = try C.compress([1],options:.init(format:.lz4,provider:C.nativeProvider(.lz4),level:C.nativeProvider(.lz4).maximumLevel))
}
let directory = CommandLine.arguments[1];let output = CommandLine.arguments[2]
var decisions = 0;var cancelCount = 0;var closeCount = 0
for (name,format) in [("ZSTD",C.Format.zstd),("LZ4",C.Format.lz4)] {
    let provider = C.nativeProvider(format)
    decisions += try fixtures(directory,name,format,provider)
    try models(output,name,format,provider);try wrappers(format,provider)
    cancelCount += try cancellation(format,provider);closeCount += try closeSweep(format,provider)
}
try providers()
print("PASS Swift optional \(decisions) independent decisions, 24 outputs, \(cancelCount) cancellation boundaries, \(closeCount) close boundaries")

// Preserve exact original initializer function values and trailing-closure calls.
let oldEncode:(C.Format,Int,Int,Int,(() throws -> Void)?) -> C.EncodeOptions = C.EncodeOptions.init
let oldDecode:(C.Format,Int,Int,Int,(() throws -> Void)?) -> C.DecodeOptions = C.DecodeOptions.init
_ = oldEncode(.none,1,1,6,nil);_ = oldDecode(.none,1,1,32768,nil)
_ = C.EncodeOptions(format:.none) {};_ = C.DecodeOptions(format:.none) {}

print("PROVIDERS "+C.nativeProviderVersions())
