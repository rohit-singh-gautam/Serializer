// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.io.File

// Exercise the public policy-copy callback and recover after every failed owning invocation.
fun main(args: Array<String>) {
    val defaults=XmlLimits(maxBytes=16777216,maxNodes=200000,maxTextBytes=16777216)
    for(index in 1 until args.size) {
        val input=File(args[index]).readBytes()
        val value=XmlExampleOrder.decodeXml(input,defaults)
        val canonical=value.encode(Protocol.BINARY_NONE)
        var verified=0
        for(writing in listOf(false,true)) for(threshold in listOf(1,1000,0)) {
            var polls=0;val deadline=System.nanoTime()-1
            val marker=IllegalStateException("Caller XML interruption")
            val limits=defaults.withCancellation {
                if(++polls>=threshold&&threshold!=0||threshold==0&&System.nanoTime()>=deadline)throw marker
            }
            var failed=false
            try { if(writing)value.encodeXml(limits) else XmlExampleOrder.decodeXml(input,limits) }
            catch(failure:IllegalStateException) { check(failure===marker);failed=true }
            check(failed&&(threshold==0||polls==threshold))
            check(canonical.contentEquals(value.encode(Protocol.BINARY_NONE)))
            check(canonical.contentEquals(XmlExampleOrder.decodeXml(value.encodeXml(defaults),defaults).encode(Protocol.BINARY_NONE)))
            ++verified
        }
        File(args[0],"$index.wire").writeBytes(canonical)
        println("{\"fixture\":$index,\"verified_interruptions\":$verified,\"recovered\":true}")
    }
}
