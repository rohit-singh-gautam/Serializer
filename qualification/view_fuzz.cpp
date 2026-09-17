#include "fuzz_support.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>

namespace {
using rohit::serializer::storage_mode;
using readonly_record = fuzz_models::record<storage_mode::read_only_view>;
using mutable_record = fuzz_models::record<storage_mode::mutable_view>;

// Read mapped fields and collection iterators, then exercise only size-preserving mutations.
void exercise_view(qualification::fuzz_input& input, mutable_record& editor) {
  const auto reader =
      readonly_record::map(std::span<const std::uint8_t>{input.bytes}, input.limits);
  editor.set_enabled(!reader.get_enabled());
  editor.set_number(reader.get_number() ^ std::uint32_t{1});
  editor.set_text(std::string(reader.get_text().size(), 'x'));
  editor.set_status(reader.get_status());
  auto values = editor.get_values();
  for (auto position = values.begin(); position != values.end(); ++position) {
    position.set(*position ^ std::uint32_t{1});
  }
  for (auto child : editor.get_children()) {
    child.set_code(child.get_code() ^ std::uint32_t{1});
    child.set_label(std::string(child.get_label().size(), 'y'));
  }
  auto labels = editor.get_labels();
  for (auto position = labels.begin(); position != labels.end(); ++position) {
    const auto entry = *position;
    position.set_value(std::string(entry.second.size(), 'z'));
  }
  auto nested = editor.get_nested();
  nested.set_code(nested.get_code() ^ std::uint32_t{1});
  nested.set_label(std::string(nested.get_label().size(), 'n'));
  auto payload = editor.get_payload();
  if (payload.index() == 0) {
    payload.set<0>(payload.get<0>() ^ std::uint32_t{1});
  } else {
    // Preserve even unusual floating representations without arithmetic on signaling NaNs.
    const auto bits = std::bit_cast<std::uint32_t>(payload.get<1>());
    payload.set<1>(std::bit_cast<float>(bits));
  }

  // Mutations must retain the layout and agree with an independent owning decode.
  const auto verified =
      readonly_record::map(std::span<const std::uint8_t>{input.bytes}, input.limits);
  const auto stream = rohit::make_constant_full_stream(input.bytes.data(), input.bytes.size());
  rohit::serializer::binary_none<rohit::serializer::serialize_type::in> decoder{stream};
  qualification::owning_record value{};
  decoder.serialize_in(value);
  decoder.finish();
  if (value.enabled != verified.get_enabled() || value.number != verified.get_number() ||
      value.text != verified.get_text() || value.values.size() != verified.get_values().size() ||
      value.children.size() != verified.get_children().size() ||
      value.nested.code != verified.get_nested().get_code()) {
    std::abort();
  }
}
} // namespace

// Validate mapped input before getters, iteration, setters, and an owning-decoder cross-check.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace qualification;
  if (size < fuzz_control_bytes || size - fuzz_control_bytes > fuzz_payload_bytes) {
    return 0;
  }
  fuzz_input input{data, size};
  std::optional<mutable_record> editor;
  reject_invalid_input([&] { editor.emplace(mutable_record::map(input.bytes, input.limits)); });
  if (editor) {
    // Once mapping succeeds, failures during valid getters/setters are findings, not parse rejection.
    exercise_view(input, *editor);
  }
  return 0;
}
