// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation
typealias C = SerializerCompression
let formats: [(String,C.Format)] = [("NONE",.none),("GZIP",.gzip),("ZLIB",.zlib),("DEFLATE",.deflate),("ZSTD",.zstd),("LZ4",.lz4)]
/// Keep provider selection explicit; the core formats retain their self-contained path.
func provider(_ format:C.Format)->C.Provider? { format == .zstd || format == .lz4 ? C.nativeProvider(format) : nil }
/// Apply the same exact byte/history policies to every language's input.
func decoding(_ format:C.Format,_ input:Int=8388608,_ output:Int=8388608,_ window:Int=8388608)->C.DecodeOptions {
    if let selected=provider(format) { return .init(format:format,provider:selected,maxInputBytes:input,maxOutputBytes:output,maxWindowBytes:window) }
    return .init(format:format,maxInputBytes:input,maxOutputBytes:output,maxWindowBytes:window)
}
/// Preserve the existing options initializer while explicitly admitting optional capabilities.
func encoding(_ format:C.Format,_ input:Int=8388608,_ output:Int=8388608)->C.EncodeOptions {
    if let selected=provider(format) { return .init(format:format,provider:selected,maxInputBytes:input,maxOutputBytes:output,level:format == .lz4 ? 0 : 3) }
    return .init(format:format,maxInputBytes:input,maxOutputBytes:output,level:format == .none ? 0 : 3)
}
/// Fail release qualification independently of assertion compiler settings.
func require(_ value:Bool,_ message:String) { if !value { fatalError(message) } }
/// Read one bounded fixture only from the runner-owned corpus directory.
func read(_ root:String,_ name:String)throws->[UInt8] { Array(try Data(contentsOf:URL(fileURLWithPath:root+"/"+name))) }
/// Execute every common decision and complete owning-model exchange without SDK skips.
func run()throws {
    let args=CommandLine.arguments,root=args[2]
    if args[1]=="produce" {
        var value=AccountRecord();value.accountId=42;value.label="shared \u{03bb} \u{1f642}"
        for proto in Protocol.allCases { for (name,format) in formats {
            let stem="swift-\(proto)-"+name
            let frame=try C.encode({try value.encode(proto)},options:encoding(format))
            let plain=try value.encode(proto)
            require(try C.encode({try value.encode(proto)},options:encoding(format,plain.count,frame.count))==frame,"Exact encode admission")
            for (input,output) in [(plain.count-1,frame.count),(plain.count,frame.count-1)] {
                do { _ = try C.encode({try value.encode(proto)},options:encoding(format,input,output)); fatalError("One-tight encode admission") }
                catch let failure as C.Failure { require(failure.code == .resourceLimit,"Wrong encode admission category") }
            }
            try Data(frame).write(to:URL(fileURLWithPath:root+"/"+stem+".frame"))
            try Data(plain).write(to:URL(fileURLWithPath:root+"/"+stem+".plain"))
        }}
        print("{\"produced\":24,\"encode_bounds\":72}");return
    }
    var decisions=0,exchanges=0
    for line in try String(contentsOfFile:root+"/cases.tsv",encoding:.utf8).split(whereSeparator:{$0.isNewline}) {
        let row=line.split(separator:"\t").map(String.init);let format=formats.first{$0.0==row[1]}!.1;var observed="ok"
        do { let plain=try C.decompress(read(root,row[2]),options:decoding(format,Int(row[5])!,Int(row[6])!,Int(row[7])!))
            if row[4]=="ok" { require(try plain==read(root,row[3]),row[0]) } }
        catch let failure as C.Failure { observed=failure.code == .invalidData ? "invalid_data" : failure.code == .resourceLimit ? "resource_limit" : "unexpected" }
        require(observed==row[4],row[0]+": "+observed+" != "+row[4]);decisions+=1
    }
    for line in try String(contentsOfFile:root+"/exchange.tsv",encoding:.utf8).split(whereSeparator:{$0.isNewline}) {
        let row=line.split(separator:"\t").map(String.init),format=formats.first{$0.0==row[2]}!.1
        let proto=Protocol.allCases.first{String(describing:$0)==row[1]}!,frame=try read(root,row[3]),policy=decoding(format)
        let value=try C.decode(frame,decoder:{try AccountRecord.decode($0,proto)},options:policy)
        require(value.accountId==42&&value.label=="shared \u{03bb} \u{1f642}",row[0])
        require(try C.decompress(frame,options:policy)==read(root,row[4]),row[0]);exchanges+=1
    }
    print("{\"decisions\":\(decisions),\"exchanges\":\(exchanges)}")
}
try run()
