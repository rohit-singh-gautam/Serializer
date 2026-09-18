R"serializer(
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.CodingErrorAction

/** Existing native wire protocols; all binary scalars are little-endian. */
enum class Protocol { JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING }
// Reuse empty positional metadata for JSON map entries.
private val srlEmptyIds = intArrayOf()
/** Immutable per-message limits; collection entries accumulate across fields. */
data class Limits(val maxBytes: Int = 64*1024*1024, val maxStringBytes: Int = 16*1024*1024, val maxElements: Int = 1000000, val maxDepth: Int = 64) {
    init { require(maxBytes >= 0 && maxStringBytes >= 0 && maxElements >= 0 && maxDepth >= 0) { "Invalid decode limit" } }
}
/** Invalid input/value or exceeded resource policy; no partial decode result escapes. */
class SerializerException(message: String) : IllegalArgumentException(message)
// Reject unpaired UTF-16 surrogates before Java's encoder can substitute them.
private fun srlUtf8(value: String): ByteArray {
    var i = 0
    while (i < value.length) {
        val c = value[i++].code
        if (c in 0xd800..0xdbff) {
            if (i == value.length || value[i++].code !in 0xdc00..0xdfff) throw SerializerException("Invalid UTF-16")
        } else if (c in 0xdc00..0xdfff) throw SerializerException("Invalid UTF-16")
    }
    return value.toByteArray(Charsets.UTF_8)
}
// Compare validated text by code point, equivalent to unsigned UTF-8 byte ordering.
private fun srlCompareText(a: String, b: String): Int {
    var x = 0; var y = 0
    while (x < a.length && y < b.length) {
        val left = a.codePointAt(x); val right = b.codePointAt(y)
        if (left != right) return left.compareTo(right)
        x += Character.charCount(left); y += Character.charCount(right)
    }
    return (a.length-x).compareTo(b.length-y)
}
private data class SrlField(val json: ByteArray, val binary: ByteArray)
// Build immutable encoded schema keys once, outside per-message paths.
private fun srlField(value: String): SrlField {
    val json = SrlWriter(Protocol.JSON); json.text(value); json.raw(":")
    val binary = SrlWriter(Protocol.BINARY_STRING); binary.text(value)
    return SrlField(json.bytes(), binary.bytes())
}
internal class SrlWriter(val protocol: Protocol) {
    private val output = ByteArrayOutputStream(256)
    private var depth = 0
    // Return an independent exact-size encoding.
    fun bytes(): ByteArray = output.toByteArray()
    // Test the active protocol.
    fun json(): Boolean = protocol == Protocol.JSON
    // Append validated numeric text or punctuation.
    fun raw(value: String) { output.write(value.toByteArray(Charsets.UTF_8)) }
    // Encode Serializer's compact integer length tag.
    fun compact(value: Int) {
        if (value !in 0..0x3fffffff) throw SerializerException("Compact integer range")
        val extra = if (value <= 63) 0 else if (value <= 16383) 1 else if (value <= 4194303) 2 else 3
        output.write((extra shl 6) or (value ushr (extra*8)))
        for (i in extra-1 downTo 0) output.write(value ushr (i*8))
    }
    // Preserve declared-width integer bits and exact decimal text.
    fun integer(value: ULong, width: Int, unsigned: Boolean) {
        if (json()) {
            if (unsigned) raw(value.toString()) else { val shift = (8-width)*8; raw(((value shl shift).toLong() shr shift).toString()) }
        } else { for (i in 0 until width) output.write((value shr (i*8)).toInt()) }
    }
    // Preserve binary32 bits, including NaNs; JSON requires finite values.
    fun float32(value: Float) {
        if (json()) { if (!value.isFinite()) throw SerializerException("Nonfinite JSON number"); raw(value.toString()) }
        else integer(value.toRawBits().toUInt().toULong(), 4, true)
    }
    // Preserve binary64 bits, including signed zero.
    fun float64(value: Double) {
        if (json()) { if (!value.isFinite()) throw SerializerException("Nonfinite JSON number"); raw(value.toString()) }
        else integer(value.toRawBits().toULong(), 8, true)
    }
    // Write canonical booleans.
    fun boolean(value: Boolean) { if (json()) raw(if (value) "true" else "false") else output.write(if (value) 1 else 0) }
    // Encode strict Unicode and escape JSON control characters.
    fun text(value: String) {
        val bytes = srlUtf8(value)
        if (!json()) { compact(bytes.size); output.write(bytes); return }
        output.write(34)
        var start = 0
        for (i in bytes.indices) {
            val byte = bytes[i].toInt() and 255
            if (byte < 32 || byte == 34 || byte == 92) {
                output.write(bytes, start, i-start)
                if (byte == 34 || byte == 92) { output.write(92); output.write(byte) }
                else raw("\\u" + byte.toString(16).padStart(4, '0'))
                start = i+1
            }
        }
        output.write(bytes, start, bytes.size-start); output.write(34)
    }
    // Encode a byte-valued character; JSON is restricted to ASCII.
    fun character(value: UByte) { if (json()) { if (value.toInt() > 127) throw SerializerException("Non-ASCII JSON char"); text(value.toInt().toChar().toString()) } else output.write(value.toInt()) }
    // Select enum names only in the field contexts that use them.
    fun enumeration(value: Int, names: Array<String>, collection: Boolean) {
        if (value !in names.indices) throw SerializerException("Unknown enum")
        if (json() || (protocol == Protocol.BINARY_STRING && !collection)) text(names[value]) else compact(value)
    }
    // Bound cycles and recursive object graphs before traversal.
    fun beginObject() { depth++; if (depth > 64) throw SerializerException("Encode depth limit"); if (json()) output.write(123) }
    // Append a cached schema key without encoding its string again.
    fun field(id: Int, key: Int, first: Boolean) {
        if (json()) { if (!first) output.write(44); output.write(srlFields[key].json) }
        else if (protocol == Protocol.BINARY_INTEGER) compact(id)
        else if (protocol == Protocol.BINARY_STRING) output.write(srlFields[key].binary)
    }
    // Close protocol-specific framing and leave this object.
    fun endObject() { if (json()) output.write(125) else if (protocol != Protocol.BINARY_NONE) output.write(0); depth-- }
    // Begin a collection using its actual element count.
    fun beginArray(count: Int) { if (json()) output.write(91) else compact(count) }
    // Separate JSON entries.
    fun element(index: Int) { if (json() && index != 0) output.write(44) }
    // Finish JSON collection framing.
    fun endArray() { if (json()) output.write(93) }
}
internal data class SrlKey(val id: Int, val name: String? = null)
internal class SrlReader(val data: ByteArray, val protocol: Protocol, val limits: Limits) {
    private var offset = 0
    private var depth = 0
    private var elements = 0
    private val view = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)
    private val utf8 = Charsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT).onUnmappableCharacter(CodingErrorAction.REPORT)
    // Normalize malformed byte sequences to the codec's public validation exception.
    private fun decodeUtf8(bytes: ByteBuffer): String {
        return try { utf8.reset().decode(bytes).toString() }
            catch (error: java.nio.charset.CharacterCodingException) { throw SerializerException("Invalid UTF-8") }
    }
    init { if (data.size > limits.maxBytes) throw SerializerException("Message byte limit") }
    // Check before indexing or slicing incoming bytes.
    private fun requireBytes(count: Int) { if (count < 0 || count > data.size-offset) throw SerializerException("Truncated input") }
    // Test the active protocol.
    fun json(): Boolean = protocol == Protocol.JSON
    // Skip exactly the four JSON whitespace bytes.
    private fun whitespace() { while (offset < data.size && srlWhitespace(data[offset].toInt())) offset++ }
    // Require one JSON delimiter.
    private fun expect(byte: Int) { whitespace(); requireBytes(1); if (data[offset++].toInt() != byte) throw SerializerException("Unexpected delimiter") }
    // Consume complete compact prefixes, including accepted nonminimal forms.
    fun compact(): Int {
        requireBytes(1); val first = data[offset].toInt() and 255; val extra = first ushr 6; requireBytes(extra+1)
        var value = first and 63
        for (i in 1..extra) value = value*256 + (data[offset+i].toInt() and 255)
        offset += extra+1; return value
    }
    // Borrow token boundaries, allocating only the bounded scalar spelling.
    private fun token(): String {
        whitespace(); val start = offset
        while (offset < data.size && !srlDelimiter(data[offset].toInt())) offset++
        if (offset == start) throw SerializerException("Missing token")
        return decodeUtf8(ByteBuffer.wrap(data, start, offset-start))
    }
    // Decode exact integer bits without routing 64-bit values through Double.
    fun integer(width: Int, unsigned: Boolean): ULong {
        if (!json()) {
            requireBytes(width); val start = offset; offset += width
            return when (width) { 1 -> data[start].toUByte().toULong(); 2 -> view.getShort(start).toUShort().toULong(); 4 -> view.getInt(start).toUInt().toULong(); else -> view.getLong(start).toULong() }
        }
        val text = token()
        if (!srlInteger.matches(text) || (unsigned && text.startsWith('-'))) throw SerializerException("Invalid integer")
        if (unsigned) {
            val value = text.toULongOrNull() ?: throw SerializerException("Integer range")
            if (width < 8 && value > (ULong.MAX_VALUE shr ((8-width)*8))) throw SerializerException("Integer range")
            return value
        }
        val value = text.toLongOrNull() ?: throw SerializerException("Integer range")
        if (width < 8 && (value < -(1L shl (width*8-1)) || value >= (1L shl (width*8-1)))) throw SerializerException("Integer range")
        return value.toULong()
    }
    // Read binary32 directly and validate finite JSON range including underflow.
    fun float32(): Float {
        if (!json()) return Float.fromBits(integer(4, true).toInt())
        val text = token(); if (!srlNumber.matches(text)) throw SerializerException("Invalid number")
        val value = text.toFloatOrNull() ?: throw SerializerException("Floating-point range")
        if (!value.isFinite() || (value == 0f && srlNonzero(text))) throw SerializerException("Floating-point range")
        return value
    }
    // Read binary64 directly and validate finite JSON range including underflow.
    fun float64(): Double {
        if (!json()) return Double.fromBits(integer(8, true).toLong())
        val text = token(); if (!srlNumber.matches(text)) throw SerializerException("Invalid number")
        val value = text.toDoubleOrNull() ?: throw SerializerException("Floating-point range")
        if (!value.isFinite() || (value == 0.0 && srlNonzero(text))) throw SerializerException("Floating-point range")
        return value
    }
    // Decode canonical booleans without truthiness conversions.
    fun boolean(): Boolean {
        if (json()) return when (token()) { "true" -> true; "false" -> false; else -> throw SerializerException("Invalid boolean") }
        return when (integer(1, true)) { 0uL -> false; 1uL -> true; else -> throw SerializerException("Invalid boolean") }
    }
    // Decode four hexadecimal digits with bounds checks.
    private fun hex(): Int { requireBytes(4); var value = 0; repeat(4) { value = value*16 + ((data[offset++].toInt() and 255).toChar().digitToIntOrNull(16) ?: throw SerializerException("Invalid Unicode escape")) }; return value }
    // Decode bounded UTF-8 and strict JSON surrogate pairs.
    fun text(): String {
        if (!json()) {
            val count = compact(); if (count > limits.maxStringBytes) throw SerializerException("String limit"); requireBytes(count)
            val value = decodeUtf8(ByteBuffer.wrap(data, offset, count))
            offset += count; return value
        }
        expect(34); val start = offset; val bytes = ByteArrayOutputStream()
        while (true) {
            requireBytes(1); val byte = data[offset++].toInt() and 255
            if (offset-start-1 > limits.maxStringBytes) throw SerializerException("String limit")
            if (byte == 34) break
            if (byte < 32) throw SerializerException("Unescaped control")
            if (byte != 92) { bytes.write(byte); continue }
            requireBytes(1)
            when (val escaped = data[offset++].toInt()) {
                34, 92, 47 -> bytes.write(escaped)
                98 -> bytes.write(8); 102 -> bytes.write(12); 110 -> bytes.write(10); 114 -> bytes.write(13); 116 -> bytes.write(9)
                117 -> {
                    var code = hex()
                    if (code in 0xd800..0xdbff) {
                        requireBytes(2); if (data[offset++].toInt() != 92 || data[offset++].toInt() != 117) throw SerializerException("Invalid surrogate")
                        val low = hex(); if (low !in 0xdc00..0xdfff) throw SerializerException("Invalid surrogate")
                        code = 0x10000 + ((code-0xd800) shl 10) + low-0xdc00
                    } else if (code in 0xdc00..0xdfff) throw SerializerException("Invalid surrogate")
                    bytes.write(String(Character.toChars(code)).toByteArray(Charsets.UTF_8))
                }
                else -> throw SerializerException("Invalid string escape")
            }
        }
        if (bytes.size() > limits.maxStringBytes) throw SerializerException("String limit")
        return decodeUtf8(ByteBuffer.wrap(bytes.toByteArray()))
    }
    // Decode one ASCII JSON character or byte-valued binary character.
    fun character(): UByte { if (!json()) return integer(1, true).toUByte(); val text = text(); if (text.length != 1 || text[0].code > 127) throw SerializerException("Invalid char"); return text[0].code.toUByte() }
    // Validate enum names or ordinals against the generated declaration.
    fun enumeration(names: Array<String>, collection: Boolean): Int {
        val value = if (json() || (protocol == Protocol.BINARY_STRING && !collection)) names.indexOf(text()) else compact()
        if (value !in names.indices) throw SerializerException("Unknown enum"); return value
    }
    // Bound nesting before reading object fields.
    fun beginObject() { depth++; if (depth > limits.maxDepth) throw SerializerException("Depth limit"); if (json()) expect(123) }
    // Read positional IDs or keyed names; the caller maintains a local field cursor.
    fun key(ids: IntArray, cursor: Int): SrlKey? {
        if (protocol == Protocol.BINARY_NONE) return ids.getOrNull(cursor)?.let { SrlKey(it) }
        if (protocol == Protocol.BINARY_INTEGER) { val id = compact(); return if (id == 0) null else SrlKey(id) }
        if (json()) { whitespace(); requireBytes(1); if (data[offset].toInt() == 125) { offset++; return null }; if (cursor != 0) expect(44) }
        val name = text(); if (!json() && name.isEmpty()) return null
        if (json()) expect(58); return SrlKey(0, name)
    }
    // Leave a successfully decoded object.
    fun endObject() { depth-- }
    // Validate collection counts before allocation.
    fun beginArray(): Int { if (json()) { expect(91); return -1 }; val count = compact(); if (count > limits.maxElements-elements) throw SerializerException("Element limit"); return count }
    // Reserve primitive arrays only after their input lower bound is validated.
    fun capacity(count: Int, width: Int): Int { if (count < 0) return 0; if (width > 0 && count > (data.size-offset)/width) throw SerializerException("Truncated array"); return if (width == 0) minOf(count, 256) else count }
    // Charge collection slots and reject incomplete JSON entries.
    fun nextElement(index: Int, count: Int): Boolean {
        if (!json()) { if (index == count) return false }
        else { whitespace(); requireBytes(1); if (data[offset].toInt() == 93) { offset++; return false }; if (index != 0) expect(44) }
        if (elements == limits.maxElements) throw SerializerException("Element limit"); elements++; return true
    }
    // Require exact consumption before returning a fresh owning value.
    fun finish() { if (json()) whitespace(); if (offset != data.size) throw SerializerException("Trailing input") }
}
// Recognize whitespace without allocating temporary arrays.
private fun srlWhitespace(byte: Int): Boolean = byte == 32 || byte == 9 || byte == 10 || byte == 13
// Recognize token boundaries without per-byte allocation.
private fun srlDelimiter(byte: Int): Boolean = byte == 44 || byte == 125 || byte == 93 || srlWhitespace(byte)
private val srlInteger = Regex("-?(0|[1-9][0-9]*)")
private val srlNumber = Regex("-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?")
// Detect a nonzero mantissa to reject underflow while accepting signed zero.
private fun srlNonzero(text: String): Boolean = text.takeWhile { it != 'e' && it != 'E' }.any { it in '1'..'9' }
)serializer"
