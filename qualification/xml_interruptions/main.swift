// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation

final class Interrupted: Swift.Error {}
var defaults=XmlLimits();defaults.maxNodes=200000;defaults.maxTextBytes=16777216
let args=Array(CommandLine.arguments.dropFirst())
// Preserve the exact caller error and prove fresh owning recovery after every failure.
for index in 1..<args.count {
    let input=Array(try Data(contentsOf:URL(fileURLWithPath:args[index])))
    let value=try XmlExampleOrder.decodeXml(input,defaults)
    let canonical=try value.encode(.BINARY_NONE)
    var verified=0
    for writing in [false,true] { for threshold in [1,1000,0] {
        var polls=0;let deadline=DispatchTime.now().uptimeNanoseconds-1
        let marker=Interrupted()
        let limits=defaults.withCancellation {
            polls+=1
            if polls>=threshold&&threshold != 0 || threshold==0&&DispatchTime.now().uptimeNanoseconds>=deadline { throw marker }
        }
        var failed=false
        do { if writing { _=try value.encodeXml(limits) } else { _=try XmlExampleOrder.decodeXml(input,limits) } }
        catch { guard let failure=error as? Interrupted,failure===marker else { throw error };failed=true }
        guard failed&&(threshold==0||polls==threshold),try canonical==value.encode(.BINARY_NONE),
            try canonical==XmlExampleOrder.decodeXml(value.encodeXml(defaults),defaults).encode(.BINARY_NONE) else { fatalError("Interruption/recovery failed") }
        verified+=1
    }}
    try Data(canonical).write(to:URL(fileURLWithPath:args[0]+"/\(index).wire"))
    print("{\"fixture\":\(index),\"verified_interruptions\":\(verified),\"recovered\":true}")
}
