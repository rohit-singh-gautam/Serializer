// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/file_stream.hpp>
#include <rohit/managed.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::managed::detail {
// Validate an explicit migration in an unpublished receiver, including every retained history state.
struct migration_store_access {
  // Use private recovery construction so required references need no invalid placeholder Root value.
  template <typename Store>
  static void validate(const std::vector<std::uint8_t>& bytes, store_options options) {
    Store receiver{typename Store::recovery_tag{}, std::move(options), {}};
    receiver.load(bytes);
  }
};
} // namespace rohit::managed::detail

namespace rohit::managed::migration {
enum class payload_codec { binary_none, binary_integer, binary_string };

// Explicit policies describe the source and destination; ordinary Load never guesses either one.
struct options {
  identity_encoding source_encoding{identity_encoding::fixed};
  identity_encoding target_encoding{identity_encoding::compact_varint};
  payload_codec source_codec{payload_codec::binary_integer};
  payload_codec target_codec{payload_codec::binary_integer};
  store_options storage{};
  std::size_t max_input_bytes{serializer::decode_limits{}.max_input_bytes};
  std::size_t max_output_bytes{serializer::decode_limits{}.max_input_bytes};
  // Conservative staging/cache estimate; model allocations retain their independent decoder bounds.
  std::size_t max_working_bytes{512U * serializer::decode_limits::mebibyte};
};

// History conversion never discards retained states without an explicit application decision.
enum class history_loss_policy { reject, discard_unselected };
struct history_policy_options {
  history_mode source_mode{history_mode::linear};
  history_labels source_labels{history_labels::disabled};
  history_mode target_mode{history_mode::linear};
  history_labels target_labels{history_labels::disabled};
  history_loss_policy loss{history_loss_policy::reject};
  // A complete root-to-leaf path is required when flattening a branching tree.
  // It must include the current revision; excluded branches require discard_unselected.
  std::vector<std::uint64_t> retained_tree_path{};
};

namespace detail {
inline constexpr char stale_managed_headers_message[]{
    "Managed migration requires identity-policy-aware generated headers; regenerate with the current compiler"};

template <typename Root, typename Traits, typename = void>
struct raw_storage { using type = Root; };
template <typename Root, typename Traits>
struct raw_storage<Root, Traits, std::void_t<typename Traits::storage_type>> {
  using type = typename Traits::storage_type;
};
template <identity_encoding Encoding>
struct identity_policy {
  static constexpr bool compact_varint = Encoding == identity_encoding::compact_varint;
};

template <identity_encoding Encoding, serializer::serialize_key_type Keys>
struct wire {
  template <serializer::serialize_type Direction>
  using protocol = serializer::binary<Direction, Keys, std::endian::little, stream,
      serializer::binary_text_validation::strict, serializer::read_policy::strict,
      serializer::binary_scalar_normalization::preserve, identity_policy<Encoding>>;
};

// Apply one strict generated decoder with the caller's bounded source identity policy.
template <typename Value, identity_encoding Encoding, serializer::serialize_key_type Keys>
Value decode_wire(std::span<const std::uint8_t> bytes, serializer::decode_limits limits) {
  const auto input = make_constant_full_stream(bytes.data(), bytes.size());
  return serializer::deserialize_exact<Value, wire<Encoding, Keys>::template protocol>(input, limits);
}

// Select a source binary codec independently of the destination codec.
template <typename Value, identity_encoding Encoding>
Value decode_codec(std::span<const std::uint8_t> bytes, payload_codec codec,
                     serializer::decode_limits limits) {
  switch (codec) {
  case payload_codec::binary_none: return decode_wire<Value, Encoding, serializer::serialize_key_type::none>(bytes, limits);
  case payload_codec::binary_integer: return decode_wire<Value, Encoding, serializer::serialize_key_type::integer>(bytes, limits);
  case payload_codec::binary_string: return decode_wire<Value, Encoding, serializer::serialize_key_type::string>(bytes, limits);
  }
  throw std::invalid_argument{"Unsupported migration input codec"};
}

// Keep identity selection outside the recursive generated decode path.
template <typename Value>
Value decode_value(std::span<const std::uint8_t> bytes, identity_encoding encoding,
                     payload_codec codec, serializer::decode_limits limits) {
  switch (encoding) {
  case identity_encoding::fixed: return decode_codec<Value, identity_encoding::fixed>(bytes, codec, limits);
  case identity_encoding::compact_varint: return decode_codec<Value, identity_encoding::compact_varint>(bytes, codec, limits);
  }
  throw std::invalid_argument{"Unsupported source identity encoding"};
}

// Bound staging before encoding; no partially encoded result escapes this function.
template <typename Value, identity_encoding Encoding, serializer::serialize_key_type Keys>
std::vector<std::uint8_t> encode_wire(const Value& value, std::size_t maximum) {
  if (maximum == 0) { throw std::length_error{"Migration output-byte budget exhausted"}; }
  const stream_limits limits{0, maximum};
  full_stream_auto_alloc_limits output{&limits};
  serializer::serialize_to<wire<Encoding, Keys>::template protocol>(output, value);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Select a destination binary codec without another source decode or application mapping.
template <typename Value, identity_encoding Encoding>
std::vector<std::uint8_t> encode_codec(const Value& value, payload_codec codec, std::size_t maximum) {
  switch (codec) {
  case payload_codec::binary_none: return encode_wire<Value, Encoding, serializer::serialize_key_type::none>(value, maximum);
  case payload_codec::binary_integer: return encode_wire<Value, Encoding, serializer::serialize_key_type::integer>(value, maximum);
  case payload_codec::binary_string: return encode_wire<Value, Encoding, serializer::serialize_key_type::string>(value, maximum);
  }
  throw std::invalid_argument{"Unsupported migration output codec"};
}

// Identity policies are compile-time codec parameters after this explicit command-level selection.
template <typename Value>
std::vector<std::uint8_t> encode_value(const Value& value, identity_encoding encoding,
                                        payload_codec codec, std::size_t maximum) {
  switch (encoding) {
  case identity_encoding::fixed: return encode_codec<Value, identity_encoding::fixed>(value, codec, maximum);
  case identity_encoding::compact_varint: return encode_codec<Value, identity_encoding::compact_varint>(value, codec, maximum);
  }
  throw std::invalid_argument{"Unsupported target identity encoding"};
}

// Resolve the generated schema's default policy; handwritten traits retain their declared fixed form.
template <typename Traits>
constexpr identity_encoding native_encoding() {
  if constexpr (requires { Traits::id_encoding; }) { return Traits::id_encoding; }
  else { return identity_encoding::fixed; }
}

// Require the same generated model layout, with only its explicitly selected identity codec changed.
template <typename Traits>
std::string schema_binding(identity_encoding encoding) {
  constexpr std::string_view suffix{":compact_varint"};
  std::string result{Traits::schema_id};
  if constexpr (native_encoding<Traits>() == identity_encoding::compact_varint) {
    if (!result.ends_with(suffix)) {
      throw std::invalid_argument{"Generated compact managed binding has no identity codec marker"};
    }
    result.resize(result.size() - suffix.size());
  }
  if (encoding == identity_encoding::compact_varint) { result += suffix; }
  else if (encoding != identity_encoding::fixed) { throw std::invalid_argument{"Unsupported identity encoding"}; }
  return result;
}

// Reject zero bounds and account for the shared migration workspace using checked subtraction.
inline void charge(std::size_t& used, std::size_t amount, std::size_t maximum) {
  if (used > maximum || amount > maximum - used) {
    throw std::length_error{"Migration working-byte budget exceeded"};
  }
  used += amount;
}

// Reserve a temporary decoder allowance while its candidate and encoded replacements coexist.
struct decode_allowance {
  std::size_t& used;
  std::size_t bytes;
  // Release only the temporary allowance; retained encoded bytes remain charged to the workspace.
  ~decode_allowance() { used -= bytes; }
};

// Narrow the decoder's existing limits rather than replacing them with an unbounded migration path.
inline serializer::decode_limits input_limits(const options& selected) {
  if (selected.max_input_bytes == 0 || selected.max_output_bytes == 0 || selected.max_working_bytes == 0) {
    throw std::invalid_argument{"Migration byte bounds must be positive"};
  }
  auto limits = selected.storage.decode;
  limits.max_input_bytes = std::min(limits.max_input_bytes, selected.max_input_bytes);
  limits.max_allocation_bytes = std::min(limits.max_allocation_bytes, selected.max_working_bytes);
  return limits;
}

// Strict native metadata readers reject unknown/duplicate fields rather than silently discarding them.
struct saved_entry {
  std::string label{};
  std::vector<std::uint8_t> snapshot{};
  bool labeled{};
  std::uint32_t seen{};
  // Decode a known metadata field once, rejecting duplicates and unknown field IDs.
  template <typename Protocol> void serialize_in_member_by_identifier(Protocol& protocol, std::uint32_t id) {
    if (id < 1 || id > 2 || (seen & (1U << id))) { throw std::invalid_argument{"Unknown or duplicate saved entry field"}; }
    seen |= 1U << id;
    if (id == 1) { protocol.serialize_in(label); labeled = true; }
    else { protocol.serialize_in(snapshot); }
  }
  // Read one complete metadata object with the shared decoder limits.
  template <typename Protocol> void serialize_in(Protocol& protocol) {
    protocol.struct_serialize_in(this);
    if (!(seen & (1U << 2))) { throw std::invalid_argument{"Missing saved entry snapshot"}; }
  }
  // Emit all retained metadata using native stable integer field IDs.
  template <typename Protocol> void serialize_out(Protocol& protocol) const {
    if (labeled) { protocol.struct_serialize_out_start(std::make_pair(1U, std::cref(label))); }
    protocol.struct_serialize_out(std::make_pair(2U, std::cref(snapshot)));
    protocol.struct_serialize_out_end();
  }
};

// Tree ancestry and labels are metadata, never reconstructed from only the current snapshot.
struct saved_revision {
  std::uint64_t number{}, parent{};
  std::string label{};
  std::vector<std::uint8_t> snapshot{};
  bool labeled{};
  std::uint32_t seen{};
  // Decode a known metadata field once, rejecting duplicates and unknown field IDs.
  template <typename Protocol> void serialize_in_member_by_identifier(Protocol& protocol, std::uint32_t id) {
    if (id < 1 || id > 4 || (seen & (1U << id))) { throw std::invalid_argument{"Unknown or duplicate saved revision field"}; }
    seen |= 1U << id;
    switch (id) {
    case 1: protocol.serialize_in(number); break;
    case 2: protocol.serialize_in(parent); break;
    case 3: protocol.serialize_in(label); labeled = true; break;
    case 4: protocol.serialize_in(snapshot); break;
    default: break;
    }
  }
  // Read one complete metadata object with the shared decoder limits.
  template <typename Protocol> void serialize_in(Protocol& protocol) {
    protocol.struct_serialize_in(this);
    constexpr auto required = (1U << 1) | (1U << 2) | (1U << 4);
    if ((seen & required) != required) { throw std::invalid_argument{"Incomplete saved revision"}; }
  }
  // Emit all retained metadata using native stable integer field IDs.
  template <typename Protocol> void serialize_out(Protocol& protocol) const {
    protocol.struct_serialize_out_start(std::make_pair(1U, number));
    protocol.struct_serialize_out(std::make_pair(2U, parent));
    if (labeled) { protocol.struct_serialize_out(std::make_pair(3U, std::cref(label))); }
    protocol.struct_serialize_out(std::make_pair(4U, std::cref(snapshot)));
    protocol.struct_serialize_out_end();
  }
};

// Read the complete supported native save union without guessing the payload's identity encoding.
struct saved_document {
  std::uint32_t format_version{}, id_bits{}, mode{};
  std::string schema_id{};
  std::uint64_t document_high{}, document_low{}, allocated_id{}, revision_high_water{}, current_revision{}, cursor{};
  std::vector<std::uint8_t> current_snapshot{};
  std::vector<saved_entry> entries{};
  std::vector<saved_revision> revisions{};
  std::uint32_t seen{};
  // Decode a known metadata field once, rejecting duplicates and unknown field IDs.
  template <typename Protocol> void serialize_in_member_by_identifier(Protocol& protocol, std::uint32_t id) {
    if (id < 1 || id > 13 || (seen & (1U << id))) { throw std::invalid_argument{"Unknown or duplicate managed save field"}; }
    seen |= 1U << id;
    switch (id) {
    case 1: protocol.serialize_in(format_version); break;
    case 2: protocol.serialize_in(schema_id); break;
    case 3: protocol.serialize_in(id_bits); break;
    case 4: protocol.serialize_in(document_high); break;
    case 5: protocol.serialize_in(document_low); break;
    case 6: protocol.serialize_in(allocated_id); break;
    case 7: protocol.serialize_in(revision_high_water); break;
    case 8: protocol.serialize_in(current_revision); break;
    case 9: protocol.serialize_in(mode); break;
    case 10: protocol.serialize_in(current_snapshot); break;
    case 11: protocol.serialize_in(revisions); break;
    case 12: protocol.serialize_in(entries); break;
    case 13: protocol.serialize_in(cursor); break;
    default: break;
    }
  }
  // Read one complete metadata object with the shared decoder limits.
  template <typename Protocol> void serialize_in(Protocol& protocol) { protocol.struct_serialize_in(this); }
  // Emit all retained metadata using native stable integer field IDs.
  template <typename Protocol> void serialize_out(Protocol& protocol) const {
    protocol.struct_serialize_out_start(std::make_pair(1U, format_version));
    protocol.struct_serialize_out(std::make_pair(2U, std::cref(schema_id)));
    protocol.struct_serialize_out(std::make_pair(3U, id_bits));
    protocol.struct_serialize_out(std::make_pair(4U, document_high));
    protocol.struct_serialize_out(std::make_pair(5U, document_low));
    protocol.struct_serialize_out(std::make_pair(6U, allocated_id));
    if (mode == static_cast<std::uint32_t>(history_mode::tree)) {
      protocol.struct_serialize_out(std::make_pair(7U, revision_high_water));
      protocol.struct_serialize_out(std::make_pair(8U, current_revision));
    }
    protocol.struct_serialize_out(std::make_pair(9U, mode));
    protocol.struct_serialize_out(std::make_pair(10U, std::cref(current_snapshot)));
    if (mode == static_cast<std::uint32_t>(history_mode::tree)) {
      protocol.struct_serialize_out(std::make_pair(11U, std::cref(revisions)));
    } else {
      protocol.struct_serialize_out(std::make_pair(12U, std::cref(entries)));
      protocol.struct_serialize_out(std::make_pair(13U, cursor));
    }
    protocol.struct_serialize_out_end();
  }
  // Validate the complete source container shape before decoding or rewriting any model snapshot.
  bool validate_shape() const {
    constexpr auto common = (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4) | (1U << 5) | (1U << 6) | (1U << 9) | (1U << 10);
    constexpr auto tree_fields = common | (1U << 7) | (1U << 8) | (1U << 11);
    constexpr auto state_fields = common | (1U << 12) | (1U << 13);
    const bool tree = mode == static_cast<std::uint32_t>(history_mode::tree);
    const bool labeled = format_version != 3;
    if ((tree && (seen != tree_fields || (format_version != 1 && format_version != 3))) ||
        (!tree && (seen != state_fields || mode > static_cast<std::uint32_t>(history_mode::linear) ||
         (format_version != 3 && !(format_version == 2 && mode == 1))))) {
      throw std::invalid_argument{"Unsupported managed save format; journals and client checkpoints require their own converter"};
    }
    for (const auto& entry : entries) {
      if (entry.labeled != labeled) { throw std::invalid_argument{"Managed save entry label contract mismatch"}; }
    }
    for (const auto& revision : revisions) {
      if (revision.labeled != labeled) { throw std::invalid_argument{"Managed save revision label contract mismatch"}; }
    }
    return labeled;
  }
};

inline constexpr auto allocation_magic = ::rohit::managed::detail::identity_allocation_checkpoint_magic;

// Preserve the existing optional per-range lease ledger around a complete converted native save.
inline std::vector<std::uint8_t> wrap(std::vector<std::uint8_t> model,
    const std::optional<records::identity_allocation_checkpoint>& allocation, std::size_t maximum) {
  if (!allocation) { return model; }
  if (maximum <= allocation_magic.size()) { throw std::length_error{"Migration output-byte budget exceeded"}; }
  records::identity_allocation_checkpoint checkpoint;
  checkpoint.format_version = allocation->format_version;
  checkpoint.leases = allocation->leases;
  checkpoint.model = std::move(model);
  auto bytes = ::rohit::managed::detail::encode(checkpoint, maximum - allocation_magic.size());
  bytes.insert(bytes.begin(), allocation_magic.begin(), allocation_magic.end());
  return bytes;
}

// Run the ordinary store's complete history/reference/schema validation using its native policy.
template <typename Root, typename Traits>
void validate_native(const std::vector<std::uint8_t>& bytes, const saved_document& document,
                       bool labeled, store_options selected) {
  if (document.mode == static_cast<std::uint32_t>(history_mode::tree)) {
    if (labeled) { ::rohit::managed::detail::migration_store_access::validate<model_store<Root, history_mode::tree, history_labels::enabled, Traits>>(bytes, selected); }
    else { ::rohit::managed::detail::migration_store_access::validate<model_store<Root, history_mode::tree, history_labels::disabled, Traits>>(bytes, selected); }
  } else if (document.mode == static_cast<std::uint32_t>(history_mode::linear)) {
    if (labeled) { ::rohit::managed::detail::migration_store_access::validate<model_store<Root, history_mode::linear, history_labels::enabled, Traits>>(bytes, selected); }
    else { ::rohit::managed::detail::migration_store_access::validate<model_store<Root, history_mode::linear, history_labels::disabled, Traits>>(bytes, selected); }
  } else { ::rohit::managed::detail::migration_store_access::validate<model_store<Root, history_mode::disabled, history_labels::disabled, Traits>>(bytes, selected); }
}
} // namespace detail

// Transcode one raw generated storage value; framing/codec/source identity policy must be explicit.
// This validates the generated wire schema. A raw model has no saved document/history envelope.
template <typename Root, typename Traits = model_traits<Root>>
std::vector<std::uint8_t> migrate_raw(std::span<const std::uint8_t> bytes, const options& selected = {}) {
  if constexpr (requires { typename Traits::storage_type; } && !requires { Traits::id_encoding; }) {
    throw std::invalid_argument{detail::stale_managed_headers_message};
  } else {
  auto limits = detail::input_limits(selected);
  if (bytes.size() > limits.max_input_bytes) { throw std::length_error{"Migration input-byte budget exceeded"}; }
  std::size_t workspace{};
  detail::charge(workspace, bytes.size(), selected.max_working_bytes);
  limits.max_allocation_bytes = std::min(limits.max_allocation_bytes,
      (selected.max_working_bytes - workspace) / 2U);
  detail::charge(workspace, limits.max_allocation_bytes, selected.max_working_bytes);
  detail::decode_allowance release_allowance{workspace, limits.max_allocation_bytes};
  const auto value = detail::decode_value<typename detail::raw_storage<Root, Traits>::type>(bytes,
      selected.source_encoding, selected.source_codec, limits);
  return detail::encode_value(value, selected.target_encoding, selected.target_codec,
      std::min(selected.max_output_bytes, selected.max_working_bytes - workspace));
  }
}

// Convert every retained native snapshot, preserving IDs/document, history branches/cursors/labels and leases.
// Only the explicitly selected persistent-ID policy changes; unsupported containers fail before publication.
template <typename Root, typename Traits = model_traits<Root>>
std::vector<std::uint8_t> migrate_save(std::span<const std::uint8_t> bytes, const options& selected = {}) {
  if constexpr (!requires { typename Traits::storage_type; Traits::schema_id; typename Traits::id_type; }) {
    throw std::invalid_argument{"Managed save migration requires a generated managed root; use --kind raw for ordinary models"};
  } else if constexpr (!requires { Traits::id_encoding; }) {
    throw std::invalid_argument{detail::stale_managed_headers_message};
  } else {
  auto limits = detail::input_limits(selected);
  if (selected.source_codec != payload_codec::binary_integer || selected.target_codec != payload_codec::binary_integer) {
    throw std::invalid_argument{"Managed saves use binary_integer snapshots; format selection applies only to raw models"};
  }
  if (bytes.size() > limits.max_input_bytes) { throw std::length_error{"Migration input-byte budget exceeded"}; }
  std::size_t workspace{};
  // Admit the source container and both document snapshot sets before parsing/cloning their payloads.
  detail::charge(workspace, bytes.size(), selected.max_working_bytes);
  detail::charge(workspace, bytes.size(), selected.max_working_bytes);
  detail::charge(workspace, bytes.size(), selected.max_working_bytes);
  limits.max_allocation_bytes = std::min(limits.max_allocation_bytes,
      selected.max_working_bytes - workspace);
  std::optional<records::identity_allocation_checkpoint> allocation;
  if (bytes.size() >= detail::allocation_magic.size() && std::ranges::equal(bytes.first(detail::allocation_magic.size()), detail::allocation_magic)) {
    allocation = ::rohit::managed::detail::decode<records::identity_allocation_checkpoint>(bytes.subspan(detail::allocation_magic.size()), limits);
    if (allocation->format_version != 1 || allocation->leases.empty()) { throw std::invalid_argument{"Unsupported identity lease checkpoint"}; }
    bytes = allocation->model;
  }
  auto document = ::rohit::managed::detail::decode<detail::saved_document>(bytes, limits);
  const bool labeled = document.validate_shape();
  if (document.schema_id != detail::schema_binding<Traits>(selected.source_encoding) ||
      document.id_bits != std::numeric_limits<typename Traits::id_type>::digits) {
    throw std::invalid_argument{"Migration source schema/identity encoding does not match the generated model"};
  }
  if (document.entries.size() > selected.storage.max_revisions || document.revisions.size() > selected.storage.max_revisions) {
    throw std::length_error{"Migration history-entry budget exceeded"};
  }
  auto validation = document;
  struct converted_snapshot { std::vector<std::uint8_t> target, native; };
  std::map<std::vector<std::uint8_t>, converted_snapshot> converted;
  std::size_t history_bytes{};
  const auto convert = [&](std::vector<std::uint8_t>& target, std::vector<std::uint8_t>& native) {
    if (target.size() > selected.storage.max_snapshot_bytes) { throw std::length_error{"Migration snapshot-byte budget exceeded"}; }
    const auto found = converted.find(target);
    if (found != converted.end()) {
      if (found->second.target.size() > target.size()) { detail::charge(workspace, found->second.target.size() - target.size(), selected.max_working_bytes); }
      if (found->second.native.size() > native.size()) { detail::charge(workspace, found->second.native.size() - native.size(), selected.max_working_bytes); }
      target = found->second.target; native = found->second.native; return;
    }
    auto snapshot_limits = limits;
    snapshot_limits.max_allocation_bytes = std::min(snapshot_limits.max_allocation_bytes,
        (selected.max_working_bytes - workspace) / 2U);
    detail::charge(workspace, snapshot_limits.max_allocation_bytes, selected.max_working_bytes);
    detail::decode_allowance release_allowance{workspace, snapshot_limits.max_allocation_bytes};
    auto value = detail::decode_value<typename Traits::storage_type>(target, selected.source_encoding,
        payload_codec::binary_integer, snapshot_limits);
    auto output = detail::encode_value(value, selected.target_encoding, payload_codec::binary_integer,
        std::min(selected.storage.max_snapshot_bytes, selected.max_working_bytes - workspace));
    detail::charge(workspace, output.size(), selected.max_working_bytes);
    std::vector<std::uint8_t> canonical;
    if (selected.target_encoding == detail::native_encoding<Traits>()) {
      detail::charge(workspace, output.size(), selected.max_working_bytes);
      canonical = output;
    } else {
      canonical = detail::encode_value(value, detail::native_encoding<Traits>(), payload_codec::binary_integer,
          std::min(selected.storage.max_snapshot_bytes, selected.max_working_bytes - workspace));
      detail::charge(workspace, canonical.size(), selected.max_working_bytes);
    }
    detail::charge(workspace, target.size(), selected.max_working_bytes);
    if (output.size() > target.size()) { detail::charge(workspace, output.size() - target.size(), selected.max_working_bytes); }
    if (canonical.size() > native.size()) { detail::charge(workspace, canonical.size() - native.size(), selected.max_working_bytes); }
    const auto cached = converted.emplace(target, converted_snapshot{std::move(output), std::move(canonical)}).first;
    target = cached->second.target;
    native = cached->second.native;
  };
  convert(document.current_snapshot, validation.current_snapshot);
  for (std::size_t index = 0; index < document.entries.size(); ++index) {
    detail::charge(history_bytes, document.entries[index].snapshot.size(), selected.storage.max_history_bytes);
    detail::charge(history_bytes, document.entries[index].label.size(), selected.storage.max_history_bytes);
    convert(document.entries[index].snapshot, validation.entries[index].snapshot);
  }
  for (std::size_t index = 0; index < document.revisions.size(); ++index) {
    detail::charge(history_bytes, document.revisions[index].snapshot.size(), selected.storage.max_history_bytes);
    detail::charge(history_bytes, document.revisions[index].label.size(), selected.storage.max_history_bytes);
    convert(document.revisions[index].snapshot, validation.revisions[index].snapshot);
  }
  document.schema_id = detail::schema_binding<Traits>(selected.target_encoding);
  validation.schema_id = std::string{Traits::schema_id};
  detail::charge(workspace, serializer::binary_integer_size(validation, selected.max_output_bytes), selected.max_working_bytes);
  detail::charge(workspace, serializer::binary_integer_size(document, selected.max_output_bytes), selected.max_working_bytes);
  auto native_bytes = detail::wrap(detail::encode_wire<detail::saved_document,
      identity_encoding::fixed, serializer::serialize_key_type::integer>(validation,
      selected.max_output_bytes), allocation, selected.max_output_bytes);
  auto validation_options = selected.storage;
  validation_options.max_resident_bytes = std::min(validation_options.max_resident_bytes,
      selected.max_working_bytes - workspace);
  validation_options.decode.max_allocation_bytes = std::min(validation_options.decode.max_allocation_bytes,
      selected.max_working_bytes - workspace);
  if (native_bytes.size() > validation_options.decode.max_input_bytes) {
    throw std::length_error{"Migration validation input-byte budget exceeded"};
  }
  detail::validate_native<Root, Traits>(native_bytes, validation, labeled, validation_options);
  return detail::wrap(detail::encode_wire<detail::saved_document,
      identity_encoding::fixed, serializer::serialize_key_type::integer>(document,
      selected.max_output_bytes), allocation, selected.max_output_bytes);
  }
}

// Convert explicit native history/label policies without changing model bytes or identity.
// Every source state is validated even if explicitly discarded. No files or live stores are mutated.
// Tree-to-linear conversion drops revision-number metadata; linear-to-tree assigns ordinal+1 revisions.
// Client checkpoints/outboxes and attached journals must use their own application-owned rebaseline.
// Only selected.storage and byte budgets apply here; identity/codec transcoding uses migrate_save.
template <typename Root, typename Traits = model_traits<Root>>
std::vector<std::uint8_t> convert_history_policy(std::span<const std::uint8_t> source,
    const history_policy_options& policy, const options& selected = {}) {
  const auto valid_mode = [](history_mode mode) {
    return mode == history_mode::disabled || mode == history_mode::linear || mode == history_mode::tree;
  };
  const auto valid_labels = [](history_labels labels) {
    return labels == history_labels::disabled || labels == history_labels::enabled;
  };
  if (!valid_mode(policy.source_mode) || !valid_mode(policy.target_mode) ||
      !valid_labels(policy.source_labels) || !valid_labels(policy.target_labels) ||
      (policy.source_mode == history_mode::disabled && policy.source_labels != history_labels::disabled) ||
      (policy.target_mode == history_mode::disabled && policy.target_labels != history_labels::disabled) ||
      (policy.loss != history_loss_policy::reject && policy.loss != history_loss_policy::discard_unselected) ||
      (!policy.retained_tree_path.empty() &&
       (policy.source_mode != history_mode::tree || policy.target_mode != history_mode::linear))) {
    throw std::invalid_argument{"Invalid explicit managed history conversion policy"};
  }
  auto limits = detail::input_limits(selected);
  if (source.size() > limits.max_input_bytes) { throw std::length_error{"History conversion input-byte budget exceeded"}; }
  std::size_t workspace{};
  // Source bytes, decoded metadata/snapshots and native validation coexist during conversion.
  constexpr auto source_validation_staging_copies = 3;
  for (int copy = 0; copy < source_validation_staging_copies; ++copy) {
    detail::charge(workspace, source.size(), selected.max_working_bytes);
  }
  limits.max_allocation_bytes = std::min(limits.max_allocation_bytes, selected.max_working_bytes - workspace);
  auto validation_options = selected.storage;
  validation_options.decode = limits;
  validation_options.max_resident_bytes = std::min(validation_options.max_resident_bytes,
      selected.max_working_bytes - workspace);
  std::optional<records::identity_allocation_checkpoint> allocation;
  auto payload = source;
  if (source.size() >= detail::allocation_magic.size() &&
      std::ranges::equal(source.first(detail::allocation_magic.size()), detail::allocation_magic)) {
    allocation = ::rohit::managed::detail::decode<records::identity_allocation_checkpoint>(
        source.subspan(detail::allocation_magic.size()), limits);
    if (allocation->format_version != 1 || allocation->leases.empty()) {
      throw std::invalid_argument{"Unsupported history conversion identity checkpoint"};
    }
    payload = allocation->model;
  }
  auto document = ::rohit::managed::detail::decode<detail::saved_document>(payload, limits);
  const bool source_labeled = document.validate_shape();
  if (document.mode != static_cast<std::uint32_t>(policy.source_mode) ||
      source_labeled != (policy.source_labels == history_labels::enabled) ||
      document.schema_id != Traits::schema_id ||
      document.id_bits != std::numeric_limits<typename Traits::id_type>::digits) {
    throw std::invalid_argument{"History conversion source profile does not match the generated model"};
  }
  const std::vector<std::uint8_t> original{source.begin(), source.end()};
  detail::validate_native<Root, Traits>(original, document, source_labeled, validation_options);
  const bool target_labeled = policy.target_labels == history_labels::enabled;
  const auto source_count = policy.source_mode == history_mode::tree ? document.revisions.size()
      : policy.source_mode == history_mode::linear ? document.entries.size() : std::size_t{1};
  if (policy.target_mode == history_mode::disabled && source_count > 1 &&
      policy.loss != history_loss_policy::discard_unselected) {
    throw std::invalid_argument{"Disabling managed history requires explicit retained-state loss permission"};
  }
  if (policy.source_mode == history_mode::tree && policy.target_mode == history_mode::linear) {
    std::map<std::uint64_t, const detail::saved_revision*> revisions;
    std::map<std::uint64_t, std::vector<std::uint64_t>> children;
    for (const auto& revision : document.revisions) {
      revisions.emplace(revision.number, &revision);
      children[revision.parent].push_back(revision.number);
    }
    auto path = policy.retained_tree_path;
    if (path.empty()) {
      auto parent = std::uint64_t{};
      for (;;) {
        const auto found = children.find(parent);
        if (found == children.end()) { break; }
        if (found->second.size() != 1) {
          throw std::invalid_argument{"Flattening a branching history requires an explicit retained tree path"};
        }
        parent = found->second.front();
        path.push_back(parent);
      }
    }
    if (path.empty() || (path.size() != document.revisions.size() &&
        policy.loss != history_loss_policy::discard_unselected)) {
      throw std::invalid_argument{"Flattening managed history would discard unselected branches"};
    }
    document.entries.clear();
    document.entries.reserve(path.size());
    auto previous = std::uint64_t{};
    bool found_current = false;
    for (const auto number : path) {
      const auto found = revisions.find(number);
      if (found == revisions.end() || found->second->parent != previous) {
        throw std::invalid_argument{"Retained tree path is not a root-to-child history chain"};
      }
      const auto& revision = *found->second;
      if (number == document.current_revision) {
        document.cursor = document.entries.size();
        found_current = true;
      }
      detail::saved_entry entry;
      entry.snapshot = revision.snapshot;
      entry.label = target_labeled ? revision.label : std::string{};
      entry.labeled = target_labeled;
      document.entries.push_back(std::move(entry));
      previous = number;
    }
    if (!found_current || children.contains(path.back())) {
      throw std::invalid_argument{"Retained tree path must include the current revision and end at a leaf"};
    }
    document.revisions.clear();
  } else if (policy.target_mode != policy.source_mode && policy.target_mode != history_mode::disabled) {
    if (policy.source_mode == history_mode::disabled) {
      detail::saved_entry baseline;
      baseline.snapshot = document.current_snapshot;
      baseline.labeled = target_labeled;
      document.entries.push_back(std::move(baseline));
      document.cursor = 0;
    }
    if (policy.target_mode == history_mode::tree) {
      document.revisions.clear();
      document.revisions.reserve(document.entries.size());
      std::uint64_t previous{};
      for (auto& entry : document.entries) {
        if (previous == std::numeric_limits<std::uint64_t>::max()) {
          throw std::overflow_error{"History conversion revision identity exhausted"};
        }
        detail::saved_revision revision;
        revision.parent = previous;
        revision.number = ++previous;
        revision.snapshot = std::move(entry.snapshot);
        revision.label = target_labeled ? std::move(entry.label) : std::string{};
        revision.labeled = target_labeled;
        document.revisions.push_back(std::move(revision));
      }
      document.current_revision = document.cursor + 1;
      document.revision_high_water = previous;
      document.entries.clear();
    }
  }
  if (policy.target_mode == history_mode::disabled) {
    document.entries.clear();
    document.revisions.clear();
    document.cursor = 0;
  }
  for (auto& entry : document.entries) {
    if (!target_labeled) { entry.label.clear(); }
    entry.labeled = target_labeled;
  }
  for (auto& revision : document.revisions) {
    if (!target_labeled) { revision.label.clear(); }
    revision.labeled = target_labeled;
  }
  document.mode = static_cast<std::uint32_t>(policy.target_mode);
  document.format_version = target_labeled ? (policy.target_mode == history_mode::tree ? 1U : 2U) : 3U;
  const auto maximum = std::min(selected.max_output_bytes, selected.max_working_bytes - workspace);
  auto result = detail::wrap(detail::encode_wire<detail::saved_document,
      identity_encoding::fixed, serializer::serialize_key_type::integer>(document, maximum), allocation, maximum);
  detail::charge(workspace, result.size(), selected.max_working_bytes);
  validation_options.decode.max_input_bytes = std::min(selected.storage.decode.max_input_bytes, selected.max_output_bytes);
  validation_options.max_resident_bytes = std::min(selected.storage.max_resident_bytes,
      selected.max_working_bytes - workspace);
  detail::validate_native<Root, Traits>(result, document, target_labeled, validation_options);
  return result;
}

namespace detail {
// Parse a positive byte limit with overflow and trailing-character checks.
inline std::size_t size_argument(std::string_view value) {
  std::size_t result{};
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result == 0) {
    throw std::invalid_argument{"Migration byte limit must be a positive integer"};
  }
  return result;
}
// Resolve only the supported explicit identity policies.
inline identity_encoding encoding_argument(std::string_view value) {
  if (value == "fixed") { return identity_encoding::fixed; }
  if (value == "compact_varint") { return identity_encoding::compact_varint; }
  throw std::invalid_argument{"Identity encoding must be fixed or compact_varint"};
}
// Keep the binary codec registry explicit so unsupported future formats cannot be silently misread.
inline payload_codec codec_argument(std::string_view value) {
  if (value == "binary_none") { return payload_codec::binary_none; }
  if (value == "binary_integer") { return payload_codec::binary_integer; }
  if (value == "binary_string") { return payload_codec::binary_string; }
  throw std::invalid_argument{"Raw migration format must be binary_none, binary_integer or binary_string"};
}

// Interpret CLI paths as UTF-8 without relying on the deprecated C++17 u8path factory.
inline std::filesystem::path path_argument(std::string_view value) {
  std::u8string utf8;
  utf8.reserve(value.size());
  for (const auto byte : value) { utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(byte))); }
  return std::filesystem::path{utf8};
}

// Read a complete bounded file without modifying it or following executable/source references.
inline std::vector<std::uint8_t> read_file(const std::filesystem::path& path, std::size_t maximum) {
  file_stream input{path, file_open_mode::read};
  const auto length = input.size();
  if (length > maximum || length > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
    throw std::length_error{"Migration input-byte budget exceeded"};
  }
  std::vector<std::uint8_t> result(static_cast<std::size_t>(length));
  input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
  if (input.gcount() != static_cast<std::streamsize>(result.size()) || input.bad() ||
      input.peek() != std::char_traits<char>::eof()) {
    throw std::runtime_error{"Migration input changed or could not be read completely"};
  }
  return result;
}

// Publish a fully synced same-directory temporary file without replacing any existing destination.
inline void publish(const std::filesystem::path& destination, std::span<const std::uint8_t> bytes) {
  auto temporary = destination;
  temporary += ".migration-" + std::to_string(make_document_id().high) + ".tmp";
  struct cleanup {
    std::filesystem::path path;
    bool owned{};
    // Remove only a successfully created task-owned temporary after its native handle closes.
    ~cleanup() {
      if (owned) { std::error_code ignored; std::filesystem::remove(path, ignored); }
    }
  };
  cleanup remove_temporary{temporary, false};
  // Cleanup precedes the writer's scope: error unwinding closes the handle before deleting our file.
  {
    file_stream output{temporary, file_open_mode::create};
    remove_temporary.owned = true;
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
      throw std::length_error{"Migration output exceeds stream size"};
    }
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.sync();
  }
  publish_file(temporary, destination, false);
  try { sync_parent_directory(destination); }
  catch (...) { throw std::runtime_error{"Migration output was published but its directory durability is uncertain"}; }
}
} // namespace detail

// Reusable schema-linked executable entry: explicit policies, bounded files and no in-place overwrite.
template <typename Root, typename Traits = model_traits<Root>>
int run_cli(int argc, const char* const* argv) {
  try {
    options selected;
    std::string kind;
    std::filesystem::path source, destination;
    std::set<std::string> seen;
    for (int index = 1; index < argc; ++index) {
      const std::string flag{argv[index]};
      if (flag == "--help") {
        std::cout << "--kind managed|raw --from fixed|compact_varint --to fixed|compact_varint --input FILE --output NEW_FILE\n"
                     "Raw: [--input-format binary_none|binary_integer|binary_string] [--output-format ...] [--codec ...]\n"
                     "Bounds: --max-input-bytes N --max-output-bytes N --max-snapshot-bytes N --max-history-bytes N --max-working-bytes N\n";
        return 0;
      }
      if (!seen.insert(flag).second || index + 1 >= argc) { throw std::invalid_argument{"Duplicate migration option or missing value"}; }
      const std::string_view value{argv[++index]};
      if (flag == "--kind") { kind = value; }
      else if (flag == "--input") { source = detail::path_argument(value); }
      else if (flag == "--output") { destination = detail::path_argument(value); }
      else if (flag == "--from") { selected.source_encoding = detail::encoding_argument(value); }
      else if (flag == "--to") { selected.target_encoding = detail::encoding_argument(value); }
      else if (flag == "--input-format") { selected.source_codec = detail::codec_argument(value); }
      else if (flag == "--output-format") { selected.target_codec = detail::codec_argument(value); }
      else if (flag == "--codec") { selected.source_codec = selected.target_codec = detail::codec_argument(value); }
      else if (flag == "--max-input-bytes") {
        selected.max_input_bytes = detail::size_argument(value);
        selected.storage.decode.max_input_bytes = selected.max_input_bytes;
        selected.storage.decode.max_allocation_bytes = std::max(selected.storage.decode.max_allocation_bytes, selected.max_input_bytes);
      }
      else if (flag == "--max-output-bytes") { selected.max_output_bytes = detail::size_argument(value); }
      else if (flag == "--max-snapshot-bytes") { selected.storage.max_snapshot_bytes = detail::size_argument(value); }
      else if (flag == "--max-history-bytes") { selected.storage.max_history_bytes = detail::size_argument(value); }
      else if (flag == "--max-working-bytes") { selected.max_working_bytes = detail::size_argument(value); }
      else { throw std::invalid_argument{"Unknown migration option: " + flag}; }
    }
    if (kind != "managed" && kind != "raw") { throw std::invalid_argument{"Select --kind managed or raw"}; }
    if (source.empty() || destination.empty() || !seen.contains("--from") || !seen.contains("--to")) {
      throw std::invalid_argument{"Migration requires explicit --from, --to, --input and --output"};
    }
    if (seen.contains("--codec") && (seen.contains("--input-format") || seen.contains("--output-format"))) {
      throw std::invalid_argument{"Use --codec or independent input/output formats"};
    }
    if (kind == "managed" && (seen.contains("--codec") || seen.contains("--input-format") || seen.contains("--output-format"))) {
      throw std::invalid_argument{"Managed saves have a fixed native container codec; format options are raw-only"};
    }
    if (std::filesystem::weakly_canonical(source) == std::filesystem::weakly_canonical(destination) ||
        std::filesystem::exists(destination)) { throw std::invalid_argument{"Migration requires a distinct, nonexistent output file"}; }
    const auto input = detail::read_file(source, selected.max_input_bytes);
    const auto output = kind == "managed" ? migrate_save<Root, Traits>(input, selected) : migrate_raw<Root, Traits>(input, selected);
    detail::publish(destination, output);
    std::cout << "Migrated " << input.size() << " bytes to " << output.size() << " bytes\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Migration failed: " << error.what() << '\n';
    return 1;
  }
}
} // namespace rohit::managed::migration
