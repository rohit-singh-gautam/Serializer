// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/reference.hpp>
#include <rohit/serializer_creator.hpp>
#include "reference_fields.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct runtime_reference_owner {
  std::string value{};
};

struct reentrant_reference_holder {
  runtime_reference_owner owner{};
  rohit::serializer::reference_binding<runtime_reference_owner,
      rohit::serializer::reference_mode::shared> binding{};
  std::size_t* destructions{};

  // Record final owner destruction when its inline reference releases the last alias.
  ~reentrant_reference_holder() { ++*destructions; }
};

struct generated_reference_holder {
  ::reference_owner owner{};
  shared_label binding{};
  std::size_t* destructions{};

  // Observe release of a containing generated destination after its field decode completes.
  ~generated_reference_holder() { ++*destructions; }
};

constexpr std::array<std::uint32_t, 1> value_path{7};

// Build a complete stable address while keeping the path alive for every test.
rohit::serializer::reference_address_view address(std::uint64_t generation = 1) {
  return {"document", 3, generation, value_path};
}

TEST(reference_runtime, borrowed_address_changes_do_not_redirect_a_saved_binding) {
  using namespace rohit::serializer;
  const runtime_reference_owner owner{"original"};
  reference_binding<runtime_reference_owner, reference_mode::borrowed> binding;
  binding.bind(owner, address());
  EXPECT_TRUE(binding.is_bound(address()));
  EXPECT_FALSE(binding.is_bound(address(2)));
  EXPECT_THROW(binding.with_owner(address(2), [](const auto&) {}), std::logic_error);
  EXPECT_EQ(binding.with_owner(address(), [](const auto& value) { return value.value; }), "original");
  EXPECT_THROW(binding.bind(owner, {}), std::invalid_argument);
  EXPECT_TRUE(binding.is_bound(address()));
  binding.unbind();
  EXPECT_FALSE(binding.is_bound(address()));
  EXPECT_THROW(binding.with_owner(address(), [](const auto&) {}), std::logic_error);
}

TEST(reference_runtime, moving_borrowed_bindings_detaches_the_source) {
  using namespace rohit::serializer;
  using binding_type = reference_binding<runtime_reference_owner, reference_mode::borrowed>;
  const runtime_reference_owner owner{"moved"};
  binding_type original;
  original.bind(owner, address());
  binding_type moved{std::move(original)};
  EXPECT_FALSE(original.is_bound(address()));
  EXPECT_THROW(original.with_owner(address(), [](const auto&) {}), std::logic_error);
  EXPECT_TRUE(moved.is_bound(address()));
  EXPECT_EQ(moved.with_owner(address(), [](const auto& value) { return value.value; }), "moved");

  binding_type assigned;
  assigned = std::move(moved);
  EXPECT_FALSE(moved.is_bound(address()));
  EXPECT_THROW(moved.with_owner(address(), [](const auto&) {}), std::logic_error);
  EXPECT_TRUE(assigned.is_bound(address()));
  EXPECT_EQ(assigned.with_owner(address(), [](const auto& value) { return value.value; }), "moved");
}

TEST(reference_runtime, shared_binding_pins_the_owner_and_rejects_ownerless_aliases) {
  using namespace rohit::serializer;
  auto owner = std::make_shared<runtime_reference_owner>(runtime_reference_owner{"retained"});
  std::weak_ptr<const runtime_reference_owner> observer{owner};
  reference_binding<runtime_reference_owner, reference_mode::shared> binding;
  binding.bind(owner, address());
  owner.reset();
  EXPECT_FALSE(observer.expired());
  EXPECT_EQ(binding.with_owner(address(), [](const auto& value) { return value.value; }), "retained");
  const runtime_reference_owner local{"ownerless"};
  std::shared_ptr<const runtime_reference_owner> ownerless{
      std::shared_ptr<const runtime_reference_owner>{}, &local};
  EXPECT_THROW(binding.bind(ownerless, address()), std::invalid_argument);
  EXPECT_TRUE(binding.is_bound(address()));
  binding.unbind();
  EXPECT_TRUE(observer.expired());
}

TEST(reference_runtime, weak_binding_locks_only_for_the_synchronous_callback) {
  using namespace rohit::serializer;
  auto owner = std::make_shared<runtime_reference_owner>(runtime_reference_owner{"scoped"});
  reference_binding<runtime_reference_owner, reference_mode::weak> binding;
  binding.bind(std::weak_ptr<const runtime_reference_owner>{owner}, address());
  binding.with_owner(address(), [&](const auto& value) {
    owner.reset();
    EXPECT_EQ(value.value, "scoped");
    EXPECT_TRUE(binding.is_bound(address()));
  });
  EXPECT_FALSE(binding.is_bound(address()));
  EXPECT_THROW(binding.with_owner(address(), [](const auto&) {}), std::logic_error);
}

TEST(reference_runtime, a_shared_callback_keeps_storage_when_it_unbinds_itself) {
  using namespace rohit::serializer;
  reference_binding<runtime_reference_owner, reference_mode::shared> binding;
  binding.bind(std::make_shared<const runtime_reference_owner>(runtime_reference_owner{"callback"}), address());
  binding.with_owner(address(), [&](const auto& value) {
    binding.unbind();
    EXPECT_EQ(value.value, "callback");
  });
  EXPECT_FALSE(binding.is_bound(address()));
}

TEST(reference_runtime, unbind_can_destroy_the_holder_containing_the_binding) {
  std::size_t destructions{};
  auto holder = std::make_shared<reentrant_reference_holder>();
  holder->destructions = &destructions;
  holder->binding.bind(std::shared_ptr<const runtime_reference_owner>{holder, &holder->owner}, address());
  auto* binding = &holder->binding;
  const std::weak_ptr<reentrant_reference_holder> observer{holder};
  holder.reset();
  EXPECT_FALSE(observer.expired());
  // The binding is the final owner of the allocation that contains the binding itself.
  // No access through this pointer is valid after unbind returns. Sanitizers also check
  // that unbind never touches its address snapshot after triggering holder destruction.
  binding->unbind();
  EXPECT_TRUE(observer.expired());
  EXPECT_EQ(destructions, 1U);
}

TEST(reference_runtime, rebinding_can_release_the_containing_owner_after_publication) {
  using namespace rohit::serializer;
  std::size_t destructions{};
  auto holder = std::make_shared<reentrant_reference_holder>();
  holder->destructions = &destructions;
  holder->binding.bind(std::shared_ptr<const runtime_reference_owner>{holder, &holder->owner}, address());
  auto* binding = &holder->binding;
  const std::weak_ptr<reentrant_reference_holder> observer{holder};
  holder.reset();
  auto replacement = std::make_shared<const runtime_reference_owner>();
  binding->bind(replacement, address(2));
  EXPECT_TRUE(observer.expired());
  EXPECT_EQ(destructions, 1U);
  EXPECT_EQ(replacement.use_count(), 1);
}

TEST(reference_runtime, copy_and_move_assignment_release_old_owners_only_after_complete_publication) {
  using namespace rohit::serializer;
  for (const bool move : std::array{false, true}) {
    SCOPED_TRACE(move);
    std::size_t destructions{};
    auto holder = std::make_shared<reentrant_reference_holder>();
    holder->destructions = &destructions;
    holder->binding.bind(std::shared_ptr<const runtime_reference_owner>{holder, &holder->owner}, address());
    auto* binding = &holder->binding;
    const std::weak_ptr<reentrant_reference_holder> observer{holder};
    holder.reset();
    auto replacement_owner = std::make_shared<const runtime_reference_owner>();
    reference_binding<runtime_reference_owner, reference_mode::shared> replacement;
    replacement.bind(replacement_owner, address(2));
    if (move) { *binding = std::move(replacement); }
    else { *binding = replacement; }
    EXPECT_TRUE(observer.expired());
    EXPECT_EQ(destructions, 1U);
    EXPECT_EQ(replacement.is_bound(address(2)), !move);
    EXPECT_EQ(replacement_owner.use_count(), move ? 1 : 2);
  }
}

// Encode only the durable address through a selected native wire protocol.
template <template <rohit::serializer::serialize_type> class Protocol, typename Value>
std::string encode_reference(const Value& value) {
  rohit::full_stream_auto_alloc stream{};
  value.template serialize_out<Protocol>(stream);
  return {reinterpret_cast<const char*>(stream.begin()), stream.current_offset()};
}

TEST(reference_generated, borrowed_and_shared_fields_keep_owner_semantics) {
  ::reference_owner owner{};
  owner.item.label = "borrowed";
  borrowed_label borrowed{};
  borrowed.bind(owner, "document", 7, 1);
  EXPECT_EQ(borrowed.read_value(), "borrowed");
  borrowed.with_value([&](const auto& value) { EXPECT_EQ(&value, &owner.item.label); });
  owner.item.label = "updated";
  EXPECT_EQ(borrowed.read_value(), "updated");
  EXPECT_EQ(borrowed.field_path, (std::vector<std::uint32_t>{5, 7, 9}));

  auto shared_owner = std::make_shared<::reference_owner>();
  shared_owner->item.label = "shared";
  shared_owner->item.number = 11;
  shared_details shared{};
  shared.bind(shared_owner, "document", 8, 2);
  auto copied = shared.read_value();
  copied.label = "independent";
  EXPECT_EQ(shared_owner->item.label, "shared");
  shared.with_value([&](const auto& value) { EXPECT_EQ(&value, &shared_owner->item); });
  std::weak_ptr<const ::reference_owner> observer{shared_owner};
  shared_owner.reset();
  EXPECT_FALSE(observer.expired());
  EXPECT_EQ(shared.read_value().number, 11U);
  shared.unbind();
  EXPECT_TRUE(observer.expired());
}

TEST(reference_generated, weak_fields_expire_and_bad_resolution_preserves_old_binding) {
  auto owner = std::make_shared<::reference_owner>();
  owner->item.label = "weak";
  weak_label weak{};
  weak.bind(owner, "document", 7, 1);
  EXPECT_THROW(weak.resolve(owner, "wrong-document", 7, 1), std::invalid_argument);
  EXPECT_EQ(weak.read_value(), "weak");
  owner.reset();
  EXPECT_FALSE(weak.is_bound());
  EXPECT_THROW(static_cast<void>(weak.read_value()), std::logic_error);
}

TEST(reference_generated, all_protocols_restore_addresses_without_live_bindings) {
  auto owner = std::make_shared<::reference_owner>();
  owner->item.label = "never encoded as the pointee";
  const auto check = [&]<template <rohit::serializer::serialize_type> class Protocol>() {
    shared_label value{};
    value.bind(owner, "document", 7, 1);
    const auto encoded = encode_reference<Protocol>(value);
    EXPECT_EQ(encoded.find(owner->item.label), std::string::npos);
    auto input = rohit::make_constant_stream(encoded.data(), encoded.size());
    value.template serialize_in<Protocol>(input);
    EXPECT_FALSE(value.is_bound());
    EXPECT_THROW(static_cast<void>(value.read_value()), std::logic_error);
    EXPECT_EQ(value.owner_id, 7U);
    value.resolve(owner, "document", 7, 1);
    EXPECT_EQ(value.read_value(), owner->item.label);
    value.owner_id = 8;
    EXPECT_FALSE(value.is_bound());
    EXPECT_THROW(static_cast<void>(value.read_value()), std::logic_error);
  };
  check.template operator()<rohit::serializer::binary_none>();
  check.template operator()<rohit::serializer::binary_integer>();
  check.template operator()<rohit::serializer::binary_string>();
  check.template operator()<rohit::serializer::json>();
}

TEST(reference_generated, decoding_retains_a_detached_owner_through_success_and_failure) {
  const auto check = [&]<template <rohit::serializer::serialize_type> class Protocol>() {
    const auto bytes = encode_reference<Protocol>(shared_label{});
    ASSERT_FALSE(bytes.empty());
    for (const bool truncated : std::array{false, true}) {
      SCOPED_TRACE(truncated);
      std::size_t destructions{};
      auto holder = std::make_shared<generated_reference_holder>();
      holder->destructions = &destructions;
      holder->binding.bind(std::shared_ptr<const ::reference_owner>{holder, &holder->owner}, "document", 7, 1);
      auto* binding = &holder->binding;
      const std::weak_ptr<generated_reference_holder> observer{holder};
      holder.reset();
      const auto size = truncated ? bytes.size() - 1 : bytes.size();
      auto input = rohit::make_constant_stream(bytes.data(), size);
      Protocol<rohit::serializer::serialize_type::in> decoder{input};
      if (truncated) { EXPECT_ANY_THROW(binding->serialize_in(decoder)); }
      else { binding->serialize_in(decoder); }
      EXPECT_TRUE(observer.expired());
      EXPECT_EQ(destructions, 1U);
    }
  };
  check.template operator()<rohit::serializer::binary_none>();
  check.template operator()<rohit::serializer::binary_integer>();
  check.template operator()<rohit::serializer::binary_string>();
  check.template operator()<rohit::serializer::json>();
}

TEST(reference_generated, null_addresses_roundtrip_and_partial_or_wrong_paths_reject) {
  shared_label value{};
  EXPECT_NO_THROW(value.validate_address());
  const auto null_bytes = encode_reference<rohit::serializer::binary_none>(value);
  auto input = rohit::make_constant_stream(null_bytes.data(), null_bytes.size());
  value.serialize_in<rohit::serializer::binary_none>(input);
  EXPECT_FALSE(value.is_bound());
  value.document_id = "partial";
  EXPECT_THROW(encode_reference<rohit::serializer::binary_none>(value), std::invalid_argument);
  value.owner_id = 7;
  value.generation = 1;
  value.field_path = {5, 7, 10};
  EXPECT_THROW(encode_reference<rohit::serializer::json>(value), std::invalid_argument);
  constexpr std::string_view wrong =
      "{\"document_id\":\"document\",\"owner_id\":7,\"generation\":1,\"field_path\":[5,7,10]}";
  auto wrong_input = rohit::make_constant_stream(wrong.data(), wrong.size());
  EXPECT_THROW(value.serialize_in<rohit::serializer::json>(wrong_input), std::invalid_argument);
}

TEST(reference_generated, constant_and_emission_output_report_unsupported_runtime_bindings) {
  constexpr std::string_view source = "serializer version 1.18.0; "
      "class a stable_ids { public uint32 b (1); } reference r = shared(a.b);";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto schema = rohit::serializer::parser::parse(input, true);
  rohit::serializer::writer::cpp_options options{};
  options.format = false;
  options.constant_evaluation = true;
  EXPECT_THROW(rohit::serializer::writer::cpp::generate(schema, options), std::invalid_argument);
  options.emission_only = true;
  options.protocols = rohit::serializer::writer::cpp_protocols::binary_none;
  EXPECT_THROW(rohit::serializer::writer::cpp::generate(schema, options), std::invalid_argument);
}

TEST(reference_generated, helper_names_cannot_collide_with_the_selected_cpp_class_name) {
  namespace writer = rohit::serializer::writer;
  constexpr std::string_view reserved[]{"bind", "resolve", "read_value", "with_value", "unbind",
      "is_bound", "validate_address", "reference_owner_type", "reference_value_type",
      "srl_reference_path_", "srl_reference_binding_type", "srl_reference_binding_",
      "srl_reference_address", "srl_reference_previous_binding", "document_id", "owner_id",
      "generation", "field_path"};
  writer::cpp_options options{};
  options.format = false;
  for (const auto name : reserved) {
    SCOPED_TRACE(name);
    const std::string source = "serializer version 1.18.0; "
        "class a stable_ids { public uint32 b (1); } reference " + std::string{name} + " = shared(a.b);";
    const auto schema = rohit::serializer::parser::parse(
        rohit::make_constant_stream(source.data(), source.size()), true);
    options.rename_identifiers = false;
    EXPECT_THROW(writer::cpp::generate(schema, options), std::invalid_argument);
  }
  constexpr std::string_view source = "serializer version 1.18.0; "
      "class a stable_ids { public uint32 b (1); } reference Bind = shared(a.b);";
  const auto schema = rohit::serializer::parser::parse(
      rohit::make_constant_stream(source.data(), source.size()), true);
  options.rename_identifiers = true;
  EXPECT_THROW(writer::cpp::generate(schema, options), std::invalid_argument);
  options.rename_identifiers = false;
  EXPECT_NE(writer::cpp::generate(schema, options).find("class Bind {"), std::string::npos);
  options.rename_identifiers = true;
  options.standard = writer::coding_standard::google;
  EXPECT_NE(writer::cpp::generate(schema, options).find("class Bind {"), std::string::npos);
}

} // namespace
