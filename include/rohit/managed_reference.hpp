// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/managed_identity.hpp>
#include <rohit/serializer.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace rohit::managed {
template <typename Plain> struct model_traits;
template <typename Root, typename Traits> class registry_view;
template <typename Root, typename Traits> class registry_edit;
template <typename Plain> class entity_read;
namespace detail {
template <typename Root, typename Node, bool Managed, typename Resolve, bool Runtime>
class editor_access;
}

// A nonowning, document-scoped native entity identity. T is a compile-time exact-type contract.
// Only checked registry lookup or decoding can construct a nonempty token; decoding does not
// establish liveness until complete-candidate validation or explicit registry resolution.
template <typename T, std::unsigned_integral Id = std::uint32_t>
class managed_ref {
  static_assert(std::same_as<Id, std::uint32_t> || std::same_as<Id, std::uint64_t>,
      "Managed references require the configured uint32 or uint64 native ID width");
  template <typename Root, typename Traits> friend class registry_view;
  template <typename Root, typename Traits> friend class registry_edit;
  template <typename Plain> friend class entity_read;
  template <typename Root, typename Node, bool Managed, typename Resolve, bool Runtime>
  friend class detail::editor_access;
  document_id document_{};
  std::uint64_t class_id_{};
  Id object_id_{};
  static constexpr std::uint32_t document_high_field = 1U;
  static constexpr std::uint32_t document_low_field = 2U;
  static constexpr std::uint32_t class_id_field = 3U;
  static constexpr std::uint32_t object_id_field = 4U;
  static constexpr std::array<std::string_view, 4> field_names{
      "document_high", "document_low", "class_id", "object_id"};
  static constexpr auto required_fields = static_cast<std::uint8_t>(
      (std::uint8_t{1U} << field_names.size()) - 1U);
  static_assert(field_names.size() < std::numeric_limits<std::uint8_t>::digits);

  // Registry friends have already checked document membership, expected type and current liveness.
  managed_ref(document_id document, Id id)
      : document_{document}, class_id_{model_traits<T>::class_id}, object_id_{id} { validate_envelope(); }

  // Follow the target model's native identity encoding; legacy handwritten traits remain fixed.
  static constexpr bool compact_object_id() {
    if constexpr (requires { model_traits<T>::id_encoding; }) {
      using encoding_type = std::remove_cv_t<decltype(model_traits<T>::id_encoding)>;
      return model_traits<T>::id_encoding == encoding_type::compact_varint;
    } else { return false; }
  }
  // Compact only the object identity; document/class metadata keeps its independent wire contract.
  auto encoded_object_id() const {
    return serializer::detail::persistent_identity_output<compact_object_id(), Id>{object_id_};
  }
  // Select the existing checked unsigned varint hook without changing JSON numeric values.
  template <typename Protocol>
  static void decode_object_id(Protocol& protocol, Id& id) {
    serializer::detail::read_persistent_identity<compact_object_id()>(protocol, id);
  }

  // A decoding adapter records required metadata separately from the durable token's storage.
  struct decoder {
    managed_ref& value;
    std::uint8_t seen{};
    // Decode one numeric metadata identity; a duplicate never conceals a conflicting value.
    template <typename Protocol>
    void serialize_in_member_by_identifier(Protocol& protocol, std::uint32_t id) {
      if (id < document_high_field || id > object_id_field) {
        throw std::invalid_argument{"Unknown managed reference metadata field"};
      }
      const auto mask = static_cast<std::uint8_t>(std::uint8_t{1U} << (id - 1U));
      if (seen & mask) { throw std::invalid_argument{"Duplicate managed reference metadata field"}; }
      seen |= mask;
      switch (id) {
      case document_high_field: protocol.template serialize_in<std::uint64_t>(value.document_.high); break;
      case document_low_field: protocol.template serialize_in<std::uint64_t>(value.document_.low); break;
      case class_id_field: protocol.template serialize_in<std::uint64_t>(value.class_id_); break;
      case object_id_field: decode_object_id(protocol, value.object_id_); break;
      default: break;
      }
    }
    // Decode stable external names independently of C++ presentation profiles.
    template <typename Protocol>
    void serialize_in_member_by_name(Protocol& protocol, std::string_view name) {
      for (std::size_t index{}; index < field_names.size(); ++index) {
        if (name == field_names[index]) {
          serialize_in_member_by_identifier(protocol, static_cast<std::uint32_t>(index + 1U)); return;
        }
      }
      throw std::invalid_argument{"Unknown managed reference metadata name"};
    }
  };
public:
  using target_type = T;
  using id_type = Id;
  static constexpr bool serializer_reuses_storage = true;
  static constexpr bool serializer_managed_reference = true;
  // A default token is deliberately invalid; ordinary optional presence represents an absent edge.
  managed_ref() noexcept = default;
  // Inspect the document namespace without resolving a pointer or acquiring an owner.
  [[nodiscard]] document_id document() const noexcept { return document_; }
  // Inspect the exact class discriminator before an optional catalog lookup.
  [[nodiscard]] std::uint64_t class_id() const noexcept { return class_id_; }
  // Preserve the store's native object ID width and value without packing class bits into it.
  [[nodiscard]] Id id() const noexcept { return object_id_; }
  // Check envelope structure and static type; membership remains a selected-snapshot operation.
  void validate_envelope() const {
    static_assert(std::same_as<Id, typename model_traits<T>::id_type>,
        "Managed reference ID width must match its exact target model");
    if ((document_.high == 0U && document_.low == 0U) || object_id_ == 0U ||
        class_id_ == 0U || class_id_ != model_traits<T>::class_id) {
      throw std::invalid_argument{"Invalid managed entity reference envelope"};
    }
  }
  // Compare identities directly; matching IDs from different documents remain different entities.
  bool operator==(const managed_ref& other) const noexcept {
    return document_.high == other.document_.high && document_.low == other.document_.low &&
        class_id_ == other.class_id_ && object_id_ == other.object_id_;
  }

  // Emit a stable optional envelope through the existing four native codecs, without changing IDs.
  template <typename Protocol> void serialize_out(Protocol& protocol) const {
    validate_envelope();
    const auto object_id = encoded_object_id();
    if constexpr (Protocol::key_type == serializer::serialize_key_type::none) {
      protocol.struct_serialize_out_start(document_.high);
      protocol.struct_serialize_out(document_.low);
      protocol.struct_serialize_out(class_id_);
      protocol.struct_serialize_out(object_id);
    } else if constexpr (Protocol::key_type == serializer::serialize_key_type::integer) {
      protocol.struct_serialize_out_start(std::make_pair(document_high_field, std::cref(document_.high)));
      protocol.struct_serialize_out(std::make_pair(document_low_field, std::cref(document_.low)));
      protocol.struct_serialize_out(std::make_pair(class_id_field, std::cref(class_id_)));
      protocol.struct_serialize_out(std::make_pair(object_id_field, std::cref(object_id)));
    } else {
      protocol.struct_serialize_out_start(std::make_pair(field_names[0], std::cref(document_.high)));
      protocol.struct_serialize_out(std::make_pair(field_names[1], std::cref(document_.low)));
      protocol.struct_serialize_out(std::make_pair(field_names[2], std::cref(class_id_)));
      protocol.struct_serialize_out(std::make_pair(field_names[3], std::cref(object_id)));
    }
    protocol.struct_serialize_out_end();
  }
  // Decode a complete envelope into a fresh candidate before replacing this token.
  template <typename Protocol> void serialize_in(Protocol& protocol) {
    managed_ref candidate{};
    if constexpr (Protocol::key_type == serializer::serialize_key_type::none) {
      [[maybe_unused]] auto scope = serializer::detail::enter_decode_object(protocol);
      protocol.template serialize_in<std::uint64_t>(candidate.document_.high);
      protocol.template serialize_in<std::uint64_t>(candidate.document_.low);
      protocol.template serialize_in<std::uint64_t>(candidate.class_id_);
      decode_object_id(protocol, candidate.object_id_);
    } else {
      decoder reader{candidate};
      protocol.template struct_serialize_in<decoder>(&reader);
      if (reader.seen != required_fields) { throw std::invalid_argument{"Managed reference metadata is incomplete"}; }
    }
    candidate.validate_envelope();
    *this = candidate;
  }
};

template <typename Value> struct runtime_traits;
namespace detail {
// Traverse declared value/ownership structure once, without following nonowning target edges.
// Generated record dispatch preserves private access and payload-version lifetimes.
template <typename Value, typename Visitor>
void visit_entity_references(Value& value, Visitor&& visitor) {
  using type = std::remove_cvref_t<Value>;
  if constexpr (requires { type::serializer_managed_reference; }) {
    visitor(value);
  } else if constexpr (requires { value.has_value(); value.value(); }) {
    if (value) { visit_entity_references(*value, visitor); }
  } else if constexpr (requires { typename type::mapped_type; }) {
    for (auto& [key, child] : value) {
      static_cast<void>(key);
      visit_entity_references(child, visitor);
    }
  } else if constexpr (requires { value.valueless_by_exception(); value.index(); }) {
    std::visit([&](auto& child) { visit_entity_references(child, visitor); }, value);
  } else if constexpr (requires { runtime_traits<type>::visit_references(value, visitor); }) {
    runtime_traits<type>::visit_references(value, visitor);
  } else if constexpr (requires { value.begin(); value.end(); }) {
    for (auto& child : value) { visit_entity_references(child, visitor); }
  }
}
} // namespace detail
} // namespace rohit::managed
