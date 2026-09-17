#include "example_support.hpp"

#include <rohit/stream.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace compression_example {

// Repeat readable paragraphs so every algorithm sees the same multi-chunk serialized input.
document make_document() {
  constexpr std::uint32_t section_count = 8;
  constexpr std::uint32_t paragraphs_per_section = 16;
  constexpr std::uint32_t repetitions_per_paragraph = 8;
  constexpr std::string_view sentence =
      "Serializer compression examples preserve every field and message boundary. ";
  document value{};
  value.revision = 1;
  value.title = "A document shared by the compression examples";
  value.labels = {{"encoding", "UTF-8"}, {"purpose", "compression example"}};
  for (std::uint32_t section_index = 0; section_index < section_count; ++section_index) {
    compression_model::section section{};
    section.number = section_index + 1;
    section.heading = "Section " + std::to_string(section.number);
    for (std::uint32_t paragraph_index = 0; paragraph_index < paragraphs_per_section;
         ++paragraph_index) {
      std::string text = "Paragraph " + std::to_string(paragraph_index + 1) + ": ";
      for (std::uint32_t repeat = 0; repeat < repetitions_per_paragraph; ++repeat) {
        text.append(sentence);
      }
      section.paragraphs.push_back(std::move(text));
    }
    value.sections.push_back(std::move(section));
  }
  return value;
}

// These budgets describe this fixture, not general-purpose application defaults.
codec::decode_limits message_limits() {
  codec::decode_limits limits{};
  limits.max_input_bytes = maximum_message_bytes;
  limits.max_string_bytes = 16 * 1024;
  limits.max_collection_elements = 1024;
  limits.max_nesting_depth = 8;
  limits.max_allocation_bytes = 4 * maximum_message_bytes;
  limits.max_work_units = 8 * maximum_message_bytes;
  return limits;
}

// Bound the original serialized message and its independently sized transport representation.
compression::encode_limits output_limits() {
  return {.max_input_bytes = maximum_message_bytes, .max_output_bytes = maximum_compressed_bytes};
}

// Compression selection never infers the schema, inner protocol, or byte order.
compression::decode_options input_options(compression::format format) {
  return {.format = format,
          .max_compressed_bytes = maximum_compressed_bytes,
          .max_decompressed_bytes = maximum_message_bytes,
          .max_window_bytes = maximum_window_bytes};
}

// Check nested fields directly, then measure the common uncompressed binary representation.
example_result verify_round_trip(const document& expected, const document& actual,
                                 std::size_t transmitted_bytes) {
  if (expected.revision != actual.revision || expected.title != actual.title ||
      expected.labels != actual.labels || expected.sections.size() != actual.sections.size()) {
    throw std::runtime_error{"Decoded document differs from its source"};
  }
  for (std::size_t index = 0; index < expected.sections.size(); ++index) {
    const auto& left = expected.sections[index];
    const auto& right = actual.sections[index];
    if (left.number != right.number || left.heading != right.heading ||
        left.paragraphs != right.paragraphs) {
      throw std::runtime_error{"Decoded section differs from its source"};
    }
  }
  rohit::full_stream_auto_alloc serialized;
  expected.serialize_out<codec::binary_integer>(serialized);
  constexpr std::size_t minimum_fixture_bytes = 64 * 1024;
  if (serialized.current_offset() <= minimum_fixture_bytes || transmitted_bytes == 0) {
    throw std::runtime_error{"Compression example fixture is unexpectedly small"};
  }
  return {serialized.current_offset(), transmitted_bytes};
}
} // namespace compression_example
