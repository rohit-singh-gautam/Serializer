// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package reproducibility

import generated.SerializerPackage as P
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path

/** Escape runtime metadata without using the generated package/model serialization path. */
private fun quote(value: String): String = buildString {
    append('"')
    for (item in value) when { item == '"' || item == '\\' -> { append('\\'); append(item) }; item.code < 32 -> append(String.format(java.util.Locale.ROOT,"\\u%04x",item.code)); else -> append(item) }
    append('"')
}

/** Identify the real JDK provider and Kotlin's existing default-level raw-DEFLATE policy. */
private fun provider(metadataMethod: Int = -1): String =
    "{\"implementation\":\"java.util.zip.Deflater\",\"version\":"+quote(Runtime.version().toString())+
    ",\"options\":{\"format\":\"raw_deflate\",\"level\":-1,\"window_bits\":15,\"nowrap\":true,\"strategy\":0,\"dictionary\":\"none\",\"flush\":\"finish\"}"+
    (if (metadataMethod < 0) "" else ",\"metadata_method\":$metadataMethod")+"}"

/** Apply the six fixed input permutations without imposing a canonical output comparator. */
private fun <T> permute(values: List<T>, permutation: Int): List<T> {
    require(permutation in 0..5); val count = values.size; if (count == 0) return emptyList()
    return when (permutation) {
        0 -> values.toList()
        1 -> values.reversed()
        2, 3 -> { val offset = if (permutation == 2) 1 else count/2; List(count) { values[(it+offset)%count] } }
        4 -> values.indices.filter { it%2 == 0 }.map { values[it] }+values.indices.filter { it%2 == 1 }.map { values[it] }
        else -> values.indices.reversed().filter { it%2 == 1 }.map { values[it] }+values.indices.reversed().filter { it%2 == 0 }.map { values[it] }
    }
}

/** Read raw UTF-8 TSV fields with empty hexadecimal payloads and authored URI text intact. */
private fun rows(path: Path, columns: Int): List<List<String>> = Files.readAllLines(path,StandardCharsets.UTF_8).filter { it.isNotEmpty() }.map { line ->
    line.split('\t').also { require(it.size == columns) { "Invalid fixture column count" } }
}

/** Decode literal hexadecimal payloads independently of generated codecs. */
private fun hex(value: String): ByteArray {
    require(value.length%2 == 0); return ByteArray(value.length/2) { value.substring(it*2,it*2+2).toInt(16).toByte() }
}

/** Produce one archive using only the existing external public package API. */
fun main(args: Array<String>) {
    if (args.size == 1 && args[0] == "--provider") { println(provider()); return }
    require(args.size in 4..5) { "Expected FIXTURES PROFILE PERMUTATION OUTPUT [FORCE_ZIP64]" }
    val profile = args[1]; require(profile in listOf("zip-stored","zip-deflate","opc-stored","opc-deflate"))
    val force = args.size == 5 && args[4] == "1"; require(args.size == 4 || args[4] in listOf("0","1"))
    val opc = profile.startsWith("opc-"); val permutation = args[2].toInt(); val compression = if (profile.endsWith("deflate")) "deflate" else "stored"
    val directory = Path.of(args[0],if (opc) "opc" else "zip")
    val parts = rows(directory.resolve("parts.tsv"),4).map { P.BinaryPart(hex(it[3]),it[0],it[1],0,compression) }
    val links = rows(directory.resolve("links.tsv"),5).map { P.Relationship(it[0],it[1],it[2],it[3],it[4]) }
    val budget = P.Budget(P.Limits(forceZip64 = force))
    val output = if (opc) P.Composer(budget).finish(P.Extensions(permute(parts,permutation),permute(links,permutation))) else P.ZipWriter(budget).use { writer ->
        for (part in permute(parts,permutation)) writer.add(part.name,part.data,part.compression)
        writer.finish()
    }
    Files.write(Path.of(args[3]),output)
    println("{\"profile\":"+quote(profile)+",\"permutation\":$permutation,\"force_zip64\":$force,\"parts\":${parts.size},\"relationships\":${if (opc) links.size else 0},\"provider\":"+provider(if (opc) 8 else 0)+"}")
}
