// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <managed_edit_dispatch.hpp>
#include <rohit/managed.hpp>
#include <rohit/serializer_creator.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
namespace managed = rohit::managed;
namespace schema = rohit::serializer;
namespace model = edit_dispatch_example;
using store_type = managed::model_store<model::derived>;
using traits = managed::model_traits<model::derived>;

// Parse complete declarations through the real language-version resolver.
auto parse(std::string_view input) {
  return schema::parser::parse(rohit::make_constant_stream(input.data(), input.size()), true);
}

// The implementation receives an editor value whose only mutation routes remain channel guarded.
template <typename Editor>
class custom_behavior final : public model::derived_edit_behavior<Editor> {
public:
  bool* entered{};
  bool fail{};
  const std::string** borrowed{};
  // Override one exact schema contract without obtaining a raw candidate pointer.
  std::int64_t adjust(Editor& receiver, std::int64_t delta) const override {
    if (entered) { *entered = true; }
    const auto result = receiver.adjust(delta);
    if (fail) { throw std::runtime_error{"Injected dispatched edit failure"}; }
    return result;
  }
  // Override an inherited concrete method at the selected exact-owner interface.
  std::int64_t reset(Editor& receiver, std::int64_t next) const override {
    receiver.base().set_value(next + 1);
    return receiver.base().get_value();
  }
  // Preserve the caller's borrowed string and return a typed result through the same guard.
  std::expected<std::int64_t, model::edit_error> checked(Editor& receiver, const std::string& input) const override {
    if (borrowed) { *borrowed = std::addressof(input); }
    return receiver.checked(input);
  }
};

// Borrowed adapters cannot bind temporary implementations or acquire mutable payload APIs.
template <typename Editor, typename Implementation>
concept temporary_binding = requires(Editor editor) { editor.bind_edit_behavior(Implementation{}); };
} // namespace

// Concrete default dispatch selects overrides and forwards inherited methods under the same transaction.
TEST(managed_edit_dispatch, nearest_override_and_inherited_default_dispatch) {
  store_type store{model::derived{}};
  store.execute_transaction([](auto& transaction) {
    auto editor = transaction.root();
    auto dispatch = editor.bind_edit_behavior();
    EXPECT_EQ(dispatch.adjust(3), 7);
    EXPECT_EQ(dispatch.reset(9), 9);
    EXPECT_EQ(dispatch.checked("hello").value(), 5);
  }).throw_if_failed();
  EXPECT_EQ(traits::payload(*store.read()).value, 5);
  store.undo();
  EXPECT_EQ(traits::payload(*store.read()).value, 1);
  store.redo();
  EXPECT_EQ(traits::payload(*store.read()).value, 5);
  managed::model_store<model::inherited> nested{model::inherited{}};
  nested.execute_transaction([](auto& transaction) {
    EXPECT_EQ(transaction.root().bind_edit_behavior().adjust(4), 9);
  }).throw_if_failed();
}

// Runtime polymorphism selects the borrowed implementation and preserves const-ref and typed errors.
TEST(managed_edit_dispatch, borrowed_implementation_and_typed_results) {
  store_type store{model::derived{}};
  store.execute_transaction([](auto& transaction) {
    auto editor = transaction.root();
    using editor_type = decltype(editor);
    using implementation_type = custom_behavior<editor_type>;
    static_assert(!temporary_binding<editor_type, implementation_type>);
    implementation_type implementation;
    const std::string* observed{};
    implementation.borrowed = &observed;
    auto dispatch = editor.bind_edit_behavior(implementation);
    EXPECT_EQ(dispatch.reset(8), 9);
    const std::string input{"caller-owned"};
    EXPECT_EQ(dispatch.checked(input).value(), static_cast<std::int64_t>(input.size()));
    EXPECT_EQ(observed, std::addressof(input));
    const auto rejected = dispatch.checked("");
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error(), model::edit_error::empty_input);
  }).throw_if_failed();
  EXPECT_EQ(traits::payload(*store.read()).value, 12);
}

// An implementation exception poisons the whole candidate even when application code catches it.
TEST(managed_edit_dispatch, implementation_failure_rolls_back_accepted_state) {
  store_type store{model::derived{}};
  const auto before = store.save();
  const auto outcome = store.execute_transaction([](auto& transaction) {
    auto editor = transaction.root();
    custom_behavior<decltype(editor)> implementation;
    implementation.fail = true;
    auto dispatch = editor.bind_edit_behavior(implementation);
    EXPECT_THROW(dispatch.adjust(100), std::runtime_error);
  });
  EXPECT_EQ(outcome.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), before);
}

// Escaped adapters fail before user implementation entry after either commit or cancellation.
TEST(managed_edit_dispatch, escaped_adapter_rejects_before_implementation_entry) {
  store_type store{model::derived{}};
  using editor_type = decltype(std::declval<store_type::transaction&>().root());
  bool entered{};
  custom_behavior<editor_type> implementation;
  implementation.entered = &entered;
  std::optional<model::derived_edit_behavior_adapter<editor_type>> escaped;
  store.execute_transaction([&](auto& transaction) {
    escaped.emplace(transaction.root().bind_edit_behavior(implementation));
    EXPECT_EQ(escaped->adjust(2), 5);
  }).throw_if_failed();
  entered = false;
  EXPECT_THROW(escaped->adjust(1), std::logic_error);
  EXPECT_FALSE(entered);
  store.execute_transaction([&](auto& transaction) {
    escaped.emplace(transaction.root().bind_edit_behavior(implementation));
    transaction.revert();
  }).throw_if_failed();
  EXPECT_THROW(escaped->adjust(1), std::logic_error);
  EXPECT_FALSE(entered);
}

// New dispatch is gated per declaration; unsupported signatures and old raw adapters remain closed.
TEST(managed_edit_dispatch, language_gating_and_safe_generated_receivers) {
  EXPECT_THROW(parse("serializer version 1.23.0; class model managed { public function virtual void edit() edit; }"),
      rohit::exception::base_parser);
  const auto source = parse("serializer version 1.24.0; class model managed { public uint64 value (1); "
      "public function abstract void edit(uint64 next) edit; }");
  schema::writer::cpp_options options;
  options.format = false;
  const auto generated = schema::writer::cpp::generate(source, options);
  EXPECT_NE(generated.find("model_edit_behavior"), std::string::npos);
  EXPECT_EQ(generated.find("class model_behavior {"), std::string::npos);
  EXPECT_EQ(generated.find("model& receiver"), std::string::npos);
  EXPECT_THROW(parse("serializer version 1.24.0; class model managed { public function virtual void edit() edit when(true); }"),
      rohit::exception::base_parser);
  EXPECT_THROW(parse("serializer version 1.24.0; class model { public function virtual void edit(string input const_ref) edit; }"),
      rohit::exception::base_parser);
}
