import java.io.File

// Dispatch concrete codecs and retain their exact resource-limit policies.
private fun decode(kind: Int, bytes: ByteArray, protocol: Protocol, limits: Limits): ByteArray = when (kind) {
    0 -> InteropMessage.decode(bytes, protocol, limits).encode(Protocol.JSON)
    1 -> CheckTextValue.decode(bytes, protocol, limits).encode(Protocol.JSON)
    2 -> CheckIntegerValue.decode(bytes, protocol, limits).encode(Protocol.JSON)
    3 -> CheckBoolValue.decode(bytes, protocol, limits).encode(Protocol.JSON)
    4 -> CheckFloatValue.decode(bytes, protocol, limits).encode(Protocol.JSON)
    5 -> CheckBytesValue.decode(bytes, protocol, limits).encode(Protocol.JSON)
    6 -> CheckEmpty.decode(bytes, protocol, limits).encode(Protocol.JSON)
    7 -> CheckRecursive.decode(bytes, protocol, limits).encode(Protocol.JSON)
    8 -> CheckVisibility.decode(bytes, protocol, limits).encode(Protocol.JSON)
    9 -> CheckDefaultValue.decode(bytes, protocol, limits).encode(Protocol.JSON)
    else -> error("Unknown test type")
}

// Keep file errors separate from expected malformed-input rejection.
fun main(args: Array<String>) {
    val directory = File(args[0]); val output = File(args[1]); val statuses = StringBuilder()
    for (line in File(directory, "manifest.tsv").readLines()) {
        val fields = line.split('\t').map { it.toInt() }
        val bytes = File(directory, "${fields[0]}.bin").readBytes()
        val limits = Limits(fields[3], fields[4], fields[5], fields[6])
        val json: ByteArray
        try { json = decode(fields[1], bytes, Protocol.entries[fields[2]], limits) }
        catch (error: IllegalArgumentException) { statuses.append("ERR\n"); continue }
        File(output, "${fields[0]}.json").writeBytes(json); statuses.append("OK\n")
    }
    File(output, "results.txt").writeText(statuses.toString())
}
