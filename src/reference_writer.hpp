// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::reference_support {

// Detect references recursively before selecting generated runtime dependencies.
inline bool has_references(const std::vector<std::unique_ptr<syntax_node>>& statements) {
  for (const auto& statement : statements) {
    if ((statement->type == object_type::class_type ||
         statement->type == object_type::generic_definition) &&
        static_cast<const class_node&>(*statement).reference) {
      return true;
    }
    if (statement->type == object_type::namespace_type &&
        has_references(static_cast<const namespace_node&>(*statement).statements)) {
      return true;
    }
  }
  return false;
}

// Detect transparent wrappers before selecting profiles that require ordinary record descriptors.
inline bool has_value_references(const std::vector<std::unique_ptr<syntax_node>>& statements) {
  for (const auto& statement : statements) {
    if ((statement->type == object_type::class_type ||
         statement->type == object_type::generic_definition) &&
        static_cast<const class_node&>(*statement).reference &&
        static_cast<const class_node&>(*statement).reference->value_projection) {
      return true;
    }
    if (statement->type == object_type::namespace_type &&
        has_value_references(static_cast<const namespace_node&>(*statement).statements)) {
      return true;
    }
  }
  return false;
}

// Emit a pointer-sized or native-handle-sized projection with no owned payload storage.
inline std::string cpp_value_methods(const class_node& owner, std::string_view owner_name,
    const std::function<std::string(const type_name&)>& type,
    const std::function<std::string(std::string_view, access_type)>& field,
    const std::function<std::string(const member&)>& storage,
    const std::function<std::string(access_type)>& access_section) {
  const auto& reference = *owner.reference;
  if (owner.member_list.size() != 1 || owner.member_list.front().name != "srl_value" || !storage) {
    throw std::invalid_argument{"Value reference projection metadata is unresolved"};
  }
  const auto& value_field = owner.member_list.front();
  const auto owner_type = type(reference.owner);
  const auto value_type = storage(value_field);
  const auto name = std::string{owner_name};
  const bool direct_value = reference.source_path.empty();
  const bool borrowed = reference.mode == reference_declaration::mode_type::borrowed;
  const bool shared = reference.mode == reference_declaration::mode_type::shared;
  const bool named_enum = value_field.modifier == member::modifier_type::none &&
      value_field.type_name_list.front().type == object_type::enum_type;
  const auto owner_parameter = borrowed ? "const " + owner_type + "&" : shared ?
      "::std::shared_ptr<const " + owner_type + ">" :
      "::std::weak_ptr<const " + owner_type + ">";
  const auto handle_type = borrowed ? "const " + value_type + "*" :
      std::string{shared ? "::std::shared_ptr<const " : "::std::weak_ptr<const "} + value_type + ">";
  std::string projection{"srl_source"};
  for (const auto& part : reference.source_path) {
    projection += "." + field(part, access_type::public_access);
  }
  const auto section = [&](access_type access) {
    return access_section ? access_section(access) :
        std::string{access == access_type::public_access ? "public:\n" : "private:\n"};
  };
  std::string output = section(access_type::private_access);
  output += "  " + handle_type + " srl_reference_value_{};\n";
  output += section(access_type::public_access);
  output += "  using reference_owner_type = " + owner_type + ";\n"
      "  using reference_value_type = " + value_type + ";\n"
      "  inline static constexpr bool serializer_reuses_storage = false;\n"
      "  // Start detached without constructing or reserving storage for a payload.\n"
      "  " + name + "() noexcept = default;\n"
      "  // Copy only the native pointer or lifetime handle.\n"
      "  " + name + "(const " + name + "&) noexcept = default;\n"
      "  // Transfer the native handle and leave the source detached.\n"
      "  " + name + "(" + name + "&& srl_other) noexcept {\n"
      "    ::std::swap(srl_reference_value_, srl_other.srl_reference_value_);\n  }\n"
      "  // Publish the complete handle before releasing an owner that can contain this wrapper.\n"
      "  " + name + "& operator=(const " + name + "& srl_other) noexcept {\n"
      "    if (this == ::std::addressof(srl_other)) { return *this; }\n"
      "    auto srl_previous = srl_other.srl_reference_value_;\n"
      "    ::std::swap(srl_reference_value_, srl_previous);\n    return *this;\n  }\n"
      "  // Move before publication and release the previous owner after evaluating the return.\n"
      "  " + name + "& operator=(" + name + "&& srl_other) noexcept {\n"
      "    if (this == ::std::addressof(srl_other)) { return *this; }\n"
      "    " + name + " srl_candidate{::std::move(srl_other)};\n"
      "    ::std::swap(srl_reference_value_, srl_candidate.srl_reference_value_);\n    return *this;\n  }\n"
      "  // Select the field once without copying it or requiring persistent identity.\n"
      "  void bind(" + owner_parameter + " srl_owner) {\n";
  if (borrowed) {
    output += "    const auto& srl_source = srl_owner;\n"
        "    srl_reference_value_ = ::std::addressof(" + projection + ");\n  }\n"
        "  // Reject a temporary owner before its selected storage can be borrowed.\n"
        "  void bind(const " + owner_type + "&&) = delete;\n"
        "  // Direct lvalue assignment borrows exactly the supplied field without an owning copy.\n"
        "  " + name + "& operator=(const reference_value_type& srl_value) noexcept {\n"
        "    srl_reference_value_ = ::std::addressof(srl_value);\n    return *this;\n  }\n"
        "  // A temporary value cannot satisfy the borrowed field lifetime.\n"
        "  " + name + "& operator=(const reference_value_type&&) = delete;\n";
  } else {
    output += shared ? "" : "    auto srl_pin = srl_owner.lock();\n";
    const std::string owner_handle = shared ? "srl_owner" : "srl_pin";
    output += "    if (!" + owner_handle + " || " + owner_handle + ".use_count() == 0) {\n"
        "      throw ::std::invalid_argument{\"Value reference needs a live owning handle\"};\n    }\n";
    if (direct_value) {
      output += "    srl_reference_value_.swap(srl_owner);\n  }\n";
    } else {
      output += "    const auto& srl_source = *" + owner_handle + ";\n"
          "    const auto* srl_selected = ::std::addressof(" + projection + ");\n"
          "    ::std::shared_ptr<const reference_value_type> srl_alias{::std::move(" + owner_handle + "), srl_selected};\n";
      if (shared) {
        output += "    srl_reference_value_.swap(srl_alias);\n  }\n";
      } else {
        output += "    ::std::weak_ptr<const reference_value_type> srl_candidate{srl_alias};\n"
            "    srl_reference_value_.swap(srl_candidate);\n  }\n";
      }
    }
  }
  output += "  // Detach the handle; no default or decoded payload is stored in this wrapper.\n"
      "  void unbind() noexcept {\n";
  output += borrowed ? "    srl_reference_value_ = nullptr;\n  }\n" :
      "    decltype(srl_reference_value_) srl_previous;\n    srl_reference_value_.swap(srl_previous);\n  }\n";
  output += "  // Report pointer availability or weak-owner expiry.\n"
      "  [[nodiscard]] bool is_bound() const noexcept { return " +
      std::string{borrowed || shared ? "static_cast<bool>(srl_reference_value_)" : "!srl_reference_value_.expired()"} + "; }\n"
      "  // Explicitly request an owning copy; ordinary encoding never calls this method.\n"
      "  [[nodiscard]] reference_value_type read_value() const {\n"
      "    const auto srl_pin = srl_reference_value_" + std::string{borrowed || shared ? ";\n" : ".lock();\n"} +
      "    if (!srl_pin) { throw ::std::logic_error{\"Value reference is unbound or expired\"}; }\n"
      "    return *srl_pin;\n  }\n"
      "  // Borrow the selected field synchronously; shared and weak handles pin it until return.\n"
      "  template <typename Callback>\n"
      "  void with_value(Callback&& srl_callback) const {\n"
      "    static_assert(::std::is_void_v<::std::invoke_result_t<Callback, const reference_value_type&>>,\n"
      "        \"Value reference callbacks must return void\");\n"
      "    const auto srl_pin = srl_reference_value_" + std::string{borrowed || shared ? ";\n" : ".lock();\n"} +
      "    if (!srl_pin) { throw ::std::logic_error{\"Value reference is unbound or expired\"}; }\n"
      "    ::std::invoke(::std::forward<Callback>(srl_callback), *srl_pin);\n  }\n"
      "  // Encode only the referenced value without creating a second owning representation.\n"
      "  template <typename SerializeOutProtocol>\n"
      "  void serialize_out(SerializeOutProtocol& srl_protocol) const {\n"
      "    [[maybe_unused]] auto srl_schema = ::rohit::serializer::detail::enter_schema_values(srl_protocol);\n"
      "    with_value([&](const reference_value_type& srl_selected) {\n"
      "      if constexpr (SerializeOutProtocol::key_type == ::rohit::serializer::serialize_key_type::string) {\n";
  output += named_enum ?
      "        srl_protocol.serialize_out(::rohit::serializer::detail::enum_name(srl_selected));\n" :
      "        srl_protocol.serialize_out(::rohit::serializer::detail::schema_named_value{srl_selected});\n";
  output += "      } else { srl_protocol.serialize_out(srl_selected); }\n    });\n  }\n"
      "  // Decode the same bytes into an ordinary owning model instead of a nonowning export.\n"
      "  template <typename SerializeInProtocol>\n"
      "  void serialize_in(SerializeInProtocol&) {\n"
      "    throw ::std::logic_error{\"Value references are export-only; decode an ordinary owning model\"};\n  }\n"
      "  // Encode through a buffer or byte sink without materializing an owning projection.\n"
      "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
      "            ::rohit::type_check::output_stream SerializerStream>\n"
      "  void serialize_out(SerializerStream& srl_stream) const {\n"
      "    ::rohit::serializer::serialize_to<Protocol>(srl_stream, *this);\n  }\n"
      "  // Reject decoding without reserving hidden owned payload storage.\n"
      "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
      "            ::rohit::type_check::input_stream SerializerStream>\n"
      "  void serialize_in(SerializerStream&&) {\n"
      "    throw ::std::logic_error{\"Value references are export-only; decode an ordinary owning model\"};\n  }\n"
      "  // Resource limits do not turn a nonowning export into an owning decode destination.\n"
      "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
      "            ::rohit::type_check::input_stream SerializerStream>\n"
      "  void serialize_in(SerializerStream&&, ::rohit::serializer::decode_limits) {\n"
      "    throw ::std::logic_error{\"Value references are export-only; decode an ordinary owning model\"};\n  }\n";
  return output;
}

// Emit immutable C++ field borrowing plus explicit address binding and resolution.
inline std::string cpp_methods(const class_node& owner, std::string_view owner_name,
    const std::function<std::string(const type_name&)>& type,
    const std::function<std::string(std::string_view, access_type)>& field,
    const std::function<std::string(access_type)>& access_section = {},
    const std::function<std::string(const member&)>& storage = {}) {
  if (!owner.reference) { return {}; }
  if (owner.reference->value_projection) {
    return cpp_value_methods(owner, owner_name, type, field, storage, access_section);
  }
  constexpr std::array helper_names{"bind", "resolve", "read_value", "with_value", "unbind",
      "is_bound", "validate_address", "reference_owner_type", "reference_value_type",
      "srl_reference_path_", "srl_reference_binding_type", "srl_reference_binding_",
      "srl_reference_address", "srl_reference_previous_binding", "document_id", "owner_id",
      "generation", "field_path"};
  if (std::find(helper_names.begin(), helper_names.end(), owner_name) != helper_names.end()) {
    throw std::invalid_argument{"Generated reference name collision: " + std::string{owner_name}};
  }
  const auto& reference = *owner.reference;
  if (reference.source_path.empty() || reference.field_path.empty()) {
    throw std::invalid_argument{"Reference projection metadata is unresolved"};
  }
  const auto owner_type = type(reference.owner);
  const auto value_type = type(reference.value_type);
  const auto mode = reference.mode == reference_declaration::mode_type::borrowed ? "borrowed" :
      reference.mode == reference_declaration::mode_type::shared ? "shared" : "weak";
  const bool borrowed = reference.mode == reference_declaration::mode_type::borrowed;
  const auto owner_parameter = borrowed ? "const " + owner_type + "&" :
      reference.mode == reference_declaration::mode_type::shared ?
          "::std::shared_ptr<const " + owner_type + ">" :
          "::std::weak_ptr<const " + owner_type + ">";
  std::string projection{"srl_owner"};
  for (const auto& part : reference.source_path) {
    projection += "." + field(part, access_type::public_access);
  }
  std::string path;
  for (const auto id : reference.field_path) {
    if (!path.empty()) { path += ", "; }
    path += std::to_string(id) + "U";
  }
  const auto section = [&](access_type access) {
    return access_section ? access_section(access) :
        std::string{access == access_type::public_access ? "public:\n" : "private:\n"};
  };
  std::string output = section(access_type::private_access);
  output += "  inline static constexpr ::std::array<::std::uint32_t, " +
      std::to_string(reference.field_path.size()) + "> srl_reference_path_{" + path + "};\n";
  output += "  using srl_reference_binding_type = ::rohit::serializer::reference_binding<" +
      owner_type + ", ::rohit::serializer::reference_mode::" + mode + ">;\n";
  output += "  srl_reference_binding_type srl_reference_binding_{};\n";
  output += "  // Borrow current portable metadata without allocating.\n"
      "  ::rohit::serializer::reference_address_view srl_reference_address() const noexcept {\n"
      "    return {document_id, owner_id, generation, field_path};\n  }\n";
  output += section(access_type::public_access);
  output += "  using reference_owner_type = " + owner_type + ";\n"
      "  using reference_value_type = " + value_type + ";\n";
  output += "  // Accept a null address or this declaration's complete canonical field address.\n"
      "  void validate_address() const {\n"
      "    if (document_id.empty() && owner_id == 0 && generation == 0 && field_path.empty()) { return; }\n"
      "    ::rohit::serializer::validate_reference_address(srl_reference_address());\n"
      "    const ::rohit::serializer::reference_address_view srl_expected{document_id, owner_id, generation, srl_reference_path_};\n"
      "    if (!::rohit::serializer::reference_address_equal(srl_reference_address(), srl_expected)) {\n"
      "      throw ::std::invalid_argument{\"Reference field path mismatch\"};\n    }\n  }\n";
  output += "  // Release the live binding; the portable address remains available for resolution.\n"
      "  void unbind() noexcept { srl_reference_binding_.unbind(); }\n"
      "  // Test exact address identity and weak-owner expiry without copying the field.\n"
      "  [[nodiscard]] bool is_bound() const noexcept {\n"
      "    return srl_reference_binding_.is_bound(srl_reference_address());\n  }\n";
  output += "  // Atomically assign this field's address and bind its immutable owner.\n"
      "  void bind(" + owner_parameter + " srl_owner, ::std::string srl_document_id,\n"
      "            ::std::uint64_t srl_owner_id, ::std::uint64_t srl_generation) {\n"
      "    ::rohit::serializer::reference_address srl_address{::std::move(srl_document_id),\n"
      "        srl_owner_id, srl_generation, {srl_reference_path_.begin(), srl_reference_path_.end()}};\n"
      "    srl_reference_binding_type srl_binding;\n"
      "    srl_binding.bind(" + std::string{borrowed ? "srl_owner" : "::std::move(srl_owner)"} +
      ", srl_address.view());\n"
      "    document_id.swap(srl_address.document_id);\n"
      "    owner_id = srl_address.owner_id; generation = srl_address.generation;\n"
      "    field_path.swap(srl_address.field_path);\n"
      "    srl_reference_binding_ = ::std::move(srl_binding);\n  }\n";
  output += "  // Resolve an existing decoded address; a mismatch preserves the previous binding.\n"
      "  void resolve(" + owner_parameter + " srl_owner, ::std::string_view srl_document_id,\n"
      "               ::std::uint64_t srl_owner_id, ::std::uint64_t srl_generation) {\n"
      "    const ::rohit::serializer::reference_address_view srl_expected{srl_document_id,\n"
      "        srl_owner_id, srl_generation, srl_reference_path_};\n"
      "    if (!::rohit::serializer::reference_address_equal(srl_reference_address(), srl_expected)) {\n"
      "      throw ::std::invalid_argument{\"Reference resolution address mismatch\"};\n    }\n"
      "    srl_reference_binding_type srl_binding;\n"
      "    srl_binding.bind(" + std::string{borrowed ? "srl_owner" : "::std::move(srl_owner)"} +
      ", srl_expected);\n"
      "    srl_reference_binding_ = ::std::move(srl_binding);\n  }\n";
  if (borrowed) {
    output += "  // Temporary owners cannot outlive a borrowed field binding.\n"
        "  void bind(const " + owner_type + "&&, ::std::string, ::std::uint64_t, ::std::uint64_t) = delete;\n"
        "  void resolve(const " + owner_type + "&&, ::std::string_view, ::std::uint64_t, ::std::uint64_t) = delete;\n";
  }
  output += "  // Return an independent owning copy of the selected field.\n"
      "  [[nodiscard]] reference_value_type read_value() const {\n"
      "    return srl_reference_binding_.with_owner(srl_reference_address(),\n"
      "        [](const reference_owner_type& srl_owner) -> reference_value_type { return " +
      projection + "; });\n  }\n";
  output += "  // Borrow the field synchronously; the callback must not retain its address.\n"
      "  template <typename Callback>\n"
      "  void with_value(Callback&& srl_callback) const {\n"
      "    static_assert(::std::is_void_v<::std::invoke_result_t<Callback, const reference_value_type&>>,\n"
      "        \"Reference callbacks must return void\");\n"
      "    srl_reference_binding_.with_owner(srl_reference_address(),\n"
      "        [&](const reference_owner_type& srl_owner) {\n"
      "          ::std::invoke(::std::forward<Callback>(srl_callback), " + projection + ");\n"
      "        });\n  }\n";
  return output;
}

} // namespace rohit::serializer::writer::reference_support
