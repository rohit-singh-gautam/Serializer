#include "fuzz_support.hpp"

#include <rohit/protobuf.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;
using qualification::owning_record;
using bytes = std::vector<std::uint8_t>;

// Generate deterministic, named seeds without touching existing discoveries or crash artifacts.
class corpus_writer {
  std::filesystem::path root;
  std::size_t count{};

public:
  // Create only the selected build-output corpus directory.
  explicit corpus_writer(const std::filesystem::path& directory) : root{directory} {
    for (const auto target : {"codec_fuzz", "view_fuzz", "protobuf_fuzz", "runtime_simd_fuzz",
                              "compression_fuzz"}) {
      std::filesystem::create_directories(root / target);
    }
  }

  // Write the harness controls followed by exact protocol bytes; I/O failures abort generation.
  void write(std::string_view target, std::string_view name, std::uint8_t selector,
             std::span<const std::uint8_t> payload, std::uint8_t policy = 0,
             std::uint8_t alignment = 0) {
    if (payload.size() > qualification::fuzz_payload_bytes) {
      throw std::length_error{"Seed exceeds the fuzz payload bound"};
    }
    std::ofstream output{root / target / name, std::ios::binary | std::ios::trunc};
    output.exceptions(std::ios::badbit | std::ios::failbit);
    const std::array controls{selector, policy, alignment};
    static_assert(controls.size() == qualification::fuzz_control_bytes);
    output.write(reinterpret_cast<const char*>(controls.data()), controls.size());
    if (!payload.empty()) {
      output.write(reinterpret_cast<const char*>(payload.data()),
                   static_cast<std::streamsize>(payload.size()));
    }
    output.close();
    ++count;
  }

  // Store one complete message, every strict prefix, and each independently restrictive budget.
  void family(std::string_view target, const std::string& name, std::uint8_t selector,
              const bytes& payload) {
    write(target, name + "_valid", selector, payload);
    for (std::size_t size = 0; size < payload.size(); ++size) {
      write(target, name + "_prefix_" + std::to_string(size), selector,
            std::span{payload}.first(size), 0, 1);
    }
    for (std::uint8_t policy = 1; policy < qualification::fuzz_policy_count; ++policy) {
      write(target, name + "_limit_" + std::to_string(policy), selector, payload, policy, 1);
    }
    for (const std::uint8_t alignment : {1, 15, 16, 31}) {
      write(target, name + "_alignment_" + std::to_string(alignment), selector, payload, 0,
            alignment);
    }
  }

  // Expose a stable seed count for verification logs.
  std::size_t size() const {
    return count;
  }
};

// Convert literal JSON, TextProto, or malformed wire text without interpreting its bytes.
bytes raw(std::string_view text) {
  return {text.begin(), text.end()};
}

// Build a record that reaches nested values, numeric arrays, string scans, enums, and a union.
owning_record fixture() {
  owning_record value{};
  value.enabled = true;
  value.number = 0x12345678;
  value.text = std::string(65, 'a') + "\n\"\\\xf0\x9f\x98\x80";
  for (std::uint32_t index = 0; index < 17; ++index) {
    value.values.push_back(index * 0x01020304);
  }
  value.nested.code = 7;
  value.nested.label = std::string(33, 'b');
  value.children = {value.nested};
  value.labels = {{1, "one"}, {2, "two"}, {3, "three"}};
  value.payload.number = 42;
  value.status = fuzz_models::phase::active;
  return value;
}

// Encode with the real generated codec and prove that each complete seed decodes under baseline limits.
template <typename Encoder, typename Decoder, typename Value>
bytes encode(const Value& value) {
  rohit::full_stream_auto_alloc output{};
  Encoder encoder{output};
  encoder.serialize_out(value);
  const auto stream = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  Decoder decoder{stream, qualification::fuzz_limits(0)};
  Value decoded{};
  decoder.serialize_in(decoded);
  decoder.finish();
  if (output.current_offset() == 0) {
    return {};
  }
  return {output.begin(), output.begin() + output.current_offset()};
}

// Add generated native records, including the opposite-endian numeric array paths.
template <codec::serialize_key_type Keys, std::endian Endian>
bytes native_record(corpus_writer& writer, std::uint8_t selector, const owning_record& value) {
  auto encoded = encode<codec::binary<codec::serialize_type::out, Keys, Endian>,
                        codec::binary<codec::serialize_type::in, Keys, Endian>>(value);
  writer.family("codec_fuzz", "record_" + std::to_string(selector), selector, encoded);
  return encoded;
}

// Cover exact compact-width transitions and their truncated prefixes independently of a record.
void compact_seeds(corpus_writer& writer) {
  for (const std::uint32_t value :
       {0U, 63U, 64U, 16383U, 16384U, 4194303U, 4194304U, 1073741823U}) {
    rohit::full_stream_auto_alloc output{};
    codec::binary_none<codec::serialize_type::out> encoder{output};
    encoder.serialize_out_variable(value);
    writer.family("codec_fuzz", "compact_" + std::to_string(value), 7,
                  {output.begin(), output.begin() + output.current_offset()});
  }
  writer.write("codec_fuzz", "compact_overlong_zero", 7, bytes{0xc0, 0, 0, 0});
}

// Put stops and UTF-8/escape boundaries on either side of SSE2/AVX2 widths and the string limit.
void simd_seeds(corpus_writer& writer) {
  constexpr std::size_t lengths[]{0,  1,  7,  8,   15,  16,  17,   31,   32,   33,
                                  63, 64, 65, 127, 128, 129, 1023, 1024, 1025, 4096};
  constexpr std::string_view unicode_pattern{
      "\xc2\x80\xe0\xa0\x80\xed\x9f\xbf\xf0\x90\x80\x80\xf4\x8f\xbf\xbf\xe4\xb8\x96"};
  for (const auto size : lengths) {
    bytes unicode(size);
    for (std::size_t index = 0; index < unicode.size(); ++index) {
      unicode[index] = static_cast<std::uint8_t>(unicode_pattern[index % unicode_pattern.size()]);
    }
    // Dense valid sequences and truncated suffixes reach the complete UTF-8 vector checks.
    writer.write("runtime_simd_fuzz", "utf8_span_" + std::to_string(size), 0, unicode, 0, 1);
    for (const auto fill : std::array<std::uint8_t, 5>{'a', ' ', '\t', 0, 255}) {
      bytes payload(size, fill);
      for (std::uint8_t width = 0; width < 3; ++width) {
        for (const std::uint8_t alignment : {0, 1, 15, 16, 31}) {
          const auto name = "span_" + std::to_string(size) + "_" + std::to_string(fill) + "_" +
                            std::to_string(width) + "_" + std::to_string(alignment);
          writer.write("runtime_simd_fuzz", name, width, payload, 0, alignment);
          if (!payload.empty()) {
            const auto last = payload.back();
            payload.back() = '"';
            writer.write("runtime_simd_fuzz", name + "_stop", width, payload, 0, alignment);
            payload.back() = last;
          }
        }
      }
    }
    if (size + 2 <= qualification::fuzz_payload_bytes) {
      writer.write("codec_fuzz", "json_string_" + std::to_string(size), 4,
                   raw("\"" + std::string(size, 'a') + "\""), 0, 1);
      writer.write("protobuf_fuzz", "json_whitespace_" + std::to_string(size), 1,
                   raw(std::string(size, ' ') + "{}"), 0, 1);
    }
  }
}

// Include independent malformed field encodings, unknown fields, and restrictive recursive cases.
void malformed_seeds(corpus_writer& writer, bytes positional) {
  positional.front() = 2;
  writer.write("codec_fuzz", "invalid_boolean", 1, positional);
  writer.write("view_fuzz", "invalid_boolean", 0, positional);
  positional.front() = 1;
  positional.back() = 63;
  writer.write("codec_fuzz", "unknown_enum", 1, positional);
  writer.write("view_fuzz", "unknown_enum", 0, positional);
  positional.back() = 1;
  constexpr std::size_t union_and_enum_bytes = 1 + sizeof(std::uint32_t) + 1;
  positional[positional.size() - union_and_enum_bytes] = 63;
  writer.write("codec_fuzz", "unknown_union", 1, positional);
  writer.write("view_fuzz", "unknown_union", 0, positional);
  writer.write("codec_fuzz", "unknown_integer_field", 2, bytes{63, 0});
  writer.write("codec_fuzz", "excessive_string_length", 2, bytes{3, 0xff, 0xff, 0xff, 0xff});
  writer.write("codec_fuzz", "excessive_array_count", 2, bytes{4, 0xff, 0xff, 0xff, 0xff});
  writer.write("codec_fuzz", "unknown_json_field", 0, raw(R"({"unknown":1})"));
  writer.write("codec_fuzz", "json_depth_limit", 6, raw("[[1],[2],[3]]"), 4);
  writer.write("codec_fuzz", "json_collection_limit", 6, raw("[[1,2,3]]"), 3);
  writer.write("codec_fuzz", "json_invalid_utf8", 4, raw("\"\xc0\xaf\""));
  writer.write("codec_fuzz", "json_unpaired_surrogate", 4, raw(R"("\uD800")"));
  writer.write("codec_fuzz", "json_escaped_unicode", 4, raw(R"("\uD83D\uDE00")"));
  writer.write("codec_fuzz", "json_number", 5, raw("-1.25e10"));
  writer.write("codec_fuzz", "json_number_overflow", 5, raw("1e99999"));
  writer.write("protobuf_fuzz", "unknown_binary_field", 0, bytes{0xf8, 0x07, 1});
  writer.write("protobuf_fuzz", "invalid_binary_tag", 0, bytes{0});
  writer.write("protobuf_fuzz", "truncated_varint", 0, bytes{0x10, 0x80});
  writer.write("protobuf_fuzz", "excessive_length", 0, bytes{0x1a, 0xff, 0xff, 0xff, 0xff, 0x0f});
  writer.write("protobuf_fuzz", "unknown_json_field", 1, raw(R"({"unknown":1})"));
  writer.write("protobuf_fuzz", "unknown_text_field", 2, raw("unknown: 1"));
  writer.write("protobuf_fuzz", "invalid_text_escape", 2, raw(R"(text: "\q")"));
  bytes groups;
  constexpr std::size_t group_depth = 10;
  for (std::size_t index = 0; index < group_depth; ++index) {
    groups.insert(groups.end(), {0xa3, 0x06});
  }
  for (std::size_t index = 0; index < group_depth; ++index) {
    groups.insert(groups.end(), {0xa4, 0x06});
  }
  writer.write("protobuf_fuzz", "unknown_group_depth", 0, groups);
}

// Generate all protocol families through the actual schema compiler's owning classes.
void compression_seeds(corpus_writer& writer, const bytes& positional) {
  namespace compression = codec::compression;
  const std::array formats{compression::format::zstd, compression::format::lz4,
                           compression::format::gzip, compression::format::zlib,
                           compression::format::deflate};
  const std::array<compression::encode_options, 5> options{
      compression::zstd_options{}, compression::lz4_options{}, compression::gzip_options{},
      compression::zlib_options{}, compression::deflate_options{}};
  for (std::uint8_t index = 0; index < formats.size(); ++index) {
    if (!compression::available(formats[index])) { continue; }
    const auto name = "frame_" + std::to_string(index);
    const auto frame = compression::compress(positional, options[index]);
    writer.family("compression_fuzz", name, index, frame);
    auto malformed = frame;
    malformed.push_back(0);
    writer.write("compression_fuzz", name + "_trailing", index, malformed);
    malformed = frame;
    malformed.insert(malformed.end(), frame.begin(), frame.end());
    writer.write("compression_fuzz", name + "_concatenated", index, malformed);
    malformed = frame;
    malformed[malformed.size() / 2] ^= 0x80;
    writer.write("compression_fuzz", name + "_corrupted", index, malformed);
    writer.family("compression_fuzz", name + "_empty", index, compression::compress({}, options[index]));
    const bytes expansion(qualification::fuzz_payload_bytes * 2, 'a');
    writer.write("compression_fuzz", name + "_expansion_limit", index,
                 compression::compress(expansion, options[index]));
  }
}

// Generate native, optional Protobuf, view, SIMD, and compiled-in compression seeds.
void generate(corpus_writer& writer) {
  const auto value = fixture();
  writer.family(
      "codec_fuzz", "record_json", 0,
      encode<codec::json<codec::serialize_type::out>, codec::json<codec::serialize_type::in>>(
          value));
  const auto positional =
      native_record<codec::serialize_key_type::none, std::endian::little>(writer, 1, value);
  native_record<codec::serialize_key_type::integer, std::endian::little>(writer, 2, value);
  native_record<codec::serialize_key_type::string, std::endian::little>(writer, 3, value);
  native_record<codec::serialize_key_type::none, std::endian::big>(writer, 8, value);
  native_record<codec::serialize_key_type::integer, std::endian::big>(writer, 9, value);
  native_record<codec::serialize_key_type::string, std::endian::big>(writer, 10, value);
  writer.family("view_fuzz", "record", 0, positional);
  writer.family("protobuf_fuzz", "record_binary", 0,
                encode<codec::protobuf_binary<codec::serialize_type::out>,
                       codec::protobuf_binary<codec::serialize_type::in>>(value));
  writer.family("protobuf_fuzz", "record_json", 1,
                encode<codec::protojson<codec::serialize_type::out>,
                       codec::protojson<codec::serialize_type::in>>(value));
  writer.family("protobuf_fuzz", "record_text", 2,
                encode<codec::textproto<codec::serialize_type::out>,
                       codec::textproto<codec::serialize_type::in>>(value));
  compact_seeds(writer);
  simd_seeds(writer);
  malformed_seeds(writer, positional);
  compression_seeds(writer, positional);
}
} // namespace

// Write a reproducible corpus beneath the supplied directory; report failures to CMake/CTest.
int main(int argc, char** argv) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: fuzz_corpus_generator OUTPUT_DIRECTORY\n";
      return 2;
    }
    corpus_writer writer{argv[1]};
    generate(writer);
    std::cout << "Wrote " << writer.size() << " deterministic seeds\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
