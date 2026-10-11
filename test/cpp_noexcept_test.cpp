// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cpp_noexcept.hpp"

#include <rohit/schema_compatibility.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// Supply a bodyless member with the exact generated exception specification.
bool cpp_noexcept_value::complete() const noexcept {
  return completed == expected;
}

// Define a namespace helper without weakening its generated no-throw contract.
bool cpp_noexcept_helpers::external_value(bool value) noexcept {
  return value;
}

// Retain the abstract behavior's contract on an external overriding implementation.
bool cpp_noexcept_implementation::complete() const noexcept {
  return enabled;
}

namespace {
namespace schema = rohit::serializer;

// Parse qualifiers with the same validation path used by the compiler executable.
auto parse_noexcept(std::string_view source) {
  return schema::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}

// Inspect unformatted signatures while preserving authored native member names.
std::string generate_noexcept(std::string_view source) {
  const auto statements = parse_noexcept(source);
  schema::writer::cpp_options options{};
  options.format = false;
  options.rename_identifiers = false;
  return schema::writer::cpp::generate(statements, options);
}

// Implement a generated abstract adapter interface with its no-throw receiver contract.
class cpp_noexcept_behavior final : public cpp_noexcept_contract_behavior {
public:
  // Read the existing model without retaining its receiver.
  bool complete(const cpp_noexcept_contract& receiver) const noexcept override {
    return receiver.enabled;
  }

  // Consume the adapter's copied string while retaining the authored implementation promise.
  bool accepts(const cpp_noexcept_contract& receiver, std::string label) const noexcept override {
    return receiver.enabled && label == "ready";
  }
};

static_assert(noexcept(std::declval<const cpp_noexcept_value&>().complete()));
static_assert(noexcept(std::declval<const cpp_noexcept_value&>().native_complete()));
static_assert(noexcept(std::declval<const cpp_noexcept_value&>().constant_complete()));
static_assert(noexcept(std::declval<const cpp_noexcept_value&>().reversed_complete()));
static_assert(noexcept(std::declval<const cpp_noexcept_value&>().echoed(std::uint64_t{7})));
static_assert(!noexcept(std::declval<const cpp_noexcept_value&>().plain_complete()));
static_assert(!noexcept(std::declval<const cpp_noexcept_value&>().forwarded()));
static_assert(noexcept(cpp_noexcept_helpers::native_value(true)));
static_assert(noexcept(cpp_noexcept_helpers::constant_value(true)));
static_assert(noexcept(cpp_noexcept_helpers::reversed_value(true)));
static_assert(noexcept(cpp_noexcept_helpers::external_value(true)));
static_assert(noexcept(std::declval<const cpp_noexcept_base&>().complete()));
static_assert(noexcept(std::declval<const cpp_noexcept_derived&>().complete()));
static_assert(noexcept(std::declval<const cpp_noexcept_implementation&>().complete()));
static_assert(noexcept(std::declval<const cpp_noexcept_generic_derived<std::uint32_t>&>().complete()));
static_assert(noexcept(std::declval<const cpp_noexcept_contract_behavior&>().complete(
    std::declval<const cpp_noexcept_contract&>())));
static_assert(noexcept(std::declval<const cpp_noexcept_contract_behavior_adapter&>().complete()));
static_assert(noexcept(std::declval<const cpp_noexcept_contract_behavior&>().accepts(
    std::declval<const cpp_noexcept_contract&>(), std::declval<std::string>())));
static_assert(!noexcept(std::declval<const cpp_noexcept_contract_behavior_adapter&>().accepts(
    std::declval<std::string>())));
static_assert(std::is_same_v<decltype(&cpp_noexcept_value::complete),
    bool (cpp_noexcept_value::*)() const noexcept>);
static_assert(std::is_same_v<decltype(&cpp_noexcept_helpers::external_value),
    bool (*)(bool) noexcept>);
static_assert(cpp_noexcept_helpers::constant_value(true));
static_assert(cpp_noexcept_helpers::reversed_value(true));
static_assert(cpp_noexcept_value{}.constant_complete());
static_assert(cpp_noexcept_value{}.reversed_complete());
} // namespace

// Native, external and abstract-adapter calls retain their authored boolean results.
TEST(cpp_noexcept, compiled_native_external_and_dispatch_contracts) {
  cpp_noexcept_value::serializer_require_behavior_definitions();
  cpp_noexcept_implementation::serializer_require_behavior_definitions();
  cpp_noexcept_value value{};
  value.completed = 3;
  value.expected = 3;
  EXPECT_TRUE(value.complete());
  EXPECT_TRUE(value.native_complete());
  EXPECT_TRUE(value.forwarded());
  EXPECT_EQ(value.echoed(std::uint64_t{7}), 7u);
  EXPECT_TRUE(cpp_noexcept_helpers::external_value(true));
  value.expected = 4;
  EXPECT_FALSE(value.complete());
  EXPECT_FALSE(value.native_complete());
  EXPECT_FALSE(value.constant_complete());
  cpp_noexcept_derived derived{};
  derived.enabled = true;
  const cpp_noexcept_base& base = derived;
  EXPECT_FALSE(base.complete());
  cpp_noexcept_contract receiver{};
  receiver.enabled = true;
  const cpp_noexcept_behavior behavior{};
  const cpp_noexcept_contract_behavior_adapter adapter{receiver, behavior};
  EXPECT_TRUE(adapter.complete());
  EXPECT_TRUE(adapter.accepts("ready"));
  EXPECT_FALSE(adapter.accepts("waiting"));
  cpp_noexcept_implementation implementation{};
  implementation.enabled = true;
  EXPECT_TRUE(implementation.complete());
  cpp_noexcept_generic_derived<std::uint32_t> generic{};
  generic.value = 3;
  const cpp_noexcept_generic_base<std::uint32_t>& generic_base = generic;
  EXPECT_FALSE(generic_base.complete());
}

// C++ no-throw and constant-evaluation promises remain independently represented in the AST.
TEST(cpp_noexcept, parses_independent_qualifiers_in_either_order) {
  const auto statements = parse_noexcept(R"SCHEMA(serializer version 1.13.0;
class sample {
  public function bool external() readonly noexcept(cpp);
  public function bool native() readonly noexcept(cpp) cpp { return true; };
  public function bool first() readonly constexpr(cpp) noexcept(cpp) cpp { return true; };
  public function bool second() readonly noexcept(cpp) constexpr(cpp) cpp { return true; };
  public function bool other() readonly noexcept(cpp) python { return True };
})SCHEMA");
  ASSERT_EQ(statements.size(), 1u);
  const auto& owner = static_cast<const schema::class_node&>(*statements.front());
  ASSERT_EQ(owner.functions.size(), 5u);
  for (const auto& method : owner.functions) {
    EXPECT_TRUE(method.cpp_noexcept);
  }
  EXPECT_FALSE(owner.functions[0].cpp_constexpr);
  EXPECT_FALSE(owner.functions[1].cpp_constexpr);
  EXPECT_TRUE(owner.functions[2].cpp_constexpr);
  EXPECT_TRUE(owner.functions[3].cpp_constexpr);
  EXPECT_TRUE(owner.functions[0].bodies.empty());
  EXPECT_EQ(owner.functions[4].bodies.front().language, "python");
}

// Malformed and misplaced qualifiers fail rather than silently changing portable failures.
TEST(cpp_noexcept, rejects_invalid_qualifiers_and_effects) {
  constexpr std::string_view sources[]{
      "serializer version 1.12.0; class sample { public function bool f() readonly noexcept(cpp); }",
      "serializer version 1; class sample { public function bool f() readonly noexcept(cpp); }",
      "serializer version 1.13.0; class sample { public function bool f() edit noexcept(cpp) cpp { return true; }; }",
      "serializer version 1.13.0; class sample { public function bool f() readonly noexcept(cpp) expression(true); }",
      "serializer version 1.13.0; class sample { public function bool f() readonly noexcept(cpp) predicate(true); }",
      "serializer version 1.13.0; class sample { public function bool f() readonly noexcept(java); }",
      "serializer version 1.13.0; class sample { public function bool f() readonly noexcept; }",
      "serializer version 1.13.0; class sample { public function bool f() readonly noexcept(); }",
      "serializer version 1.13.0; class sample { public function bool f() readonly noexcept(cpp, true); }",
      "serializer version 1.13.0; class sample { public function bool f() readonly noexcept(cpp; }",
      "serializer version 1.13.0; class sample { public function bool f() readonly noexcept(cpp) noexcept(cpp); }",
      "serializer version 1.13.0; class sample { public function bool f() readonly constexpr(cpp) noexcept(cpp) constexpr(cpp) cpp { return true; }; }",
      "serializer version 1.13.0; class sample { public function Input f<Input: integral && !bool>(Input input) readonly noexcept(cpp); }",
      "serializer version 1.13.0; function Input f<Input: integral && !bool>(Input input) readonly noexcept(cpp);"};
  for (const auto source : sources) {
    SCOPED_TRACE(source);
    EXPECT_THROW(parse_noexcept(source), std::exception);
  }
}

// Every partial qualifier/body suffix reports a parser error before publishing a declaration.
TEST(cpp_noexcept, rejects_truncated_qualifier_suffixes) {
  constexpr std::string_view source = "serializer version 1.13.0; class sample { "
      "public function bool complete() readonly noexcept(cpp) cpp { return true; }; }";
  for (auto length = source.find("noexcept"); length < source.size(); ++length) {
    SCOPED_TRACE(length);
    EXPECT_THROW(parse_noexcept(source.substr(0, length)), std::exception);
  }
}

// Ordinary managed read receivers may promise no-throw behavior without touching editor guards.
TEST(cpp_noexcept, accepts_ordinary_managed_readonly_contract) {
  const auto output = generate_noexcept("serializer version 1.13.0; "
      "class sample stable_ids managed { public bool enabled (1); "
      "public function bool complete() readonly noexcept(cpp) cpp { return true; }; }");
  EXPECT_NE(output.find("bool complete() const noexcept"), std::string::npos);
}

// External-reference checks and virtual adapters use the complete C++ function type.
TEST(cpp_noexcept, emits_noexcept_for_required_and_dispatched_signatures) {
  const auto output = generate_noexcept(R"SCHEMA(serializer version 1.13.0;
class sample {
  public function bool external() readonly noexcept(cpp);
  public function abstract bool abstracted() readonly noexcept(cpp);
}
)SCHEMA");
  EXPECT_NE(output.find("bool external() const noexcept;"), std::string::npos);
  EXPECT_NE(output.find("(SerializerReceiver::*)() const noexcept;"), std::string::npos);
  EXPECT_NE(output.find("abstracted(const sample& receiver) const noexcept = 0;"),
      std::string::npos);
  EXPECT_NE(output.find("abstracted() const noexcept(noexcept(implementation_->abstracted(*receiver_))) { return implementation_->"),
      std::string::npos);
}

// Derived native or external signatures cannot weaken an inherited explicit C++ promise.
TEST(cpp_noexcept, requires_explicit_override_preservation) {
  const std::string prefix = "serializer version 1.13.0; class base { "
      "public function virtual bool complete() readonly noexcept(cpp) cpp { return true; }; } ";
  for (const std::string_view method : {
      "public function override bool complete() readonly;",
      "public function override bool complete() readonly cpp { return false; };",
      "public function override bool complete() readonly expression(true);",
      "public function bool complete() readonly cpp { return false; };"}) {
    SCOPED_TRACE(method);
    const auto source = prefix + "class derived : public base { " + std::string{method} + " }";
    EXPECT_THROW(parse_noexcept(source), std::exception);
  }
  EXPECT_NO_THROW(parse_noexcept(prefix + "class derived : public base { "
      "public function override bool complete() readonly noexcept(cpp); }"));
  EXPECT_NO_THROW(parse_noexcept("serializer version 1.13.0; class base { "
      "public function virtual bool complete() readonly cpp { return true; }; } "
      "class derived : public base { public function override bool complete() readonly noexcept(cpp); }"));
  const std::string generic_prefix = "serializer version 1.13.0; "
      "class base<T: integral && !bool> { "
      "public function virtual bool complete() readonly noexcept(cpp) cpp { return true; }; } ";
  EXPECT_THROW(parse_noexcept(generic_prefix + "class derived<U: integral && !bool> : public base<U> { "
      "public function override bool complete() readonly cpp { return false; }; }"), std::exception);
  EXPECT_NO_THROW(parse_noexcept(generic_prefix + "class derived<U: integral && !bool> : public base<U> { "
      "public function override bool complete() readonly noexcept(cpp) cpp { return false; }; }"));
}

// Renamed and nested arguments retain their exception contract through every generic base path.
TEST(cpp_noexcept, composes_generic_inherited_exception_signatures) {
  const std::string prefix = "serializer version 1.13.0; namespace contracts { "
      "class base<T> { public function virtual bool matches(T input) readonly noexcept(cpp) "
      "cpp { (void)input; return true; }; } ";
  const std::string renamed = "class derived<U> : public base<U> { "
      "public function override bool matches(U input) readonly ";
  EXPECT_THROW(parse_noexcept(prefix + renamed + "cpp { (void)input; return false; }; } }"),
      std::exception);
  EXPECT_NO_THROW(parse_noexcept(prefix + renamed +
      "noexcept(cpp) cpp { (void)input; return false; }; } }"));
  const std::string transitive = "class middle<V> : public base<array<V>> {} "
      "class derived<W> : public middle<W> { "
      "public function override bool matches(array<W> input) readonly ";
  EXPECT_THROW(parse_noexcept(prefix + transitive + "cpp { (void)input; return false; }; } }"),
      std::exception);
  EXPECT_NO_THROW(parse_noexcept(prefix + transitive +
      "noexcept(cpp) cpp { (void)input; return false; }; } }"));
  const std::string concrete_prefix = "serializer version 1.13.0; "
      "namespace concrete { class T {} } "
      "class base<T> { public function virtual bool matches(concrete::T input) readonly "
      "noexcept(cpp) cpp { (void)input; return true; }; } ";
  const std::string concrete_derived = "class derived<U> : public base<U> { "
      "public function override bool matches(concrete::T input) readonly ";
  EXPECT_THROW(parse_noexcept(concrete_prefix + concrete_derived +
      "cpp { (void)input; return false; }; }"), std::exception);
  EXPECT_NO_THROW(parse_noexcept(concrete_prefix + concrete_derived +
      "noexcept(cpp) cpp { (void)input; return false; }; }"));
}

// Target-specific metadata does not enter other output syntax or persisted schema compatibility.
TEST(cpp_noexcept, preserves_portable_output_and_wire_contract) {
  constexpr std::string_view original = "serializer version 1.12.0; class sample stable_ids { "
      "public bool enabled (1); public function bool complete() readonly; }";
  constexpr std::string_view qualified = "serializer version 1.13.0; class sample stable_ids { "
      "public bool enabled (1); public function bool complete() readonly noexcept(cpp); }";
  const schema::parser::parsed_schema old{parse_noexcept(original), {}};
  const schema::parser::parsed_schema next{parse_noexcept(qualified), {}};
  for (const auto protocol : {schema::compatibility_protocol::json,
      schema::compatibility_protocol::binary_none, schema::compatibility_protocol::binary_integer,
      schema::compatibility_protocol::binary_string}) {
    EXPECT_TRUE(schema::check_schema_compatibility(old, next, protocol).empty());
  }
  // Hold source-map locations fixed so equality isolates only the target-specific metadata.
  auto unqualified = parse_noexcept(qualified);
  auto& owner = static_cast<schema::class_node&>(*unqualified.front());
  owner.functions.front().cpp_noexcept = false;
  const auto java = schema::writer::java::generate(next.statements, "Schema");
  EXPECT_EQ(java, schema::writer::java::generate(unqualified, "Schema"));
  EXPECT_EQ(java.find("noexcept"), std::string::npos);
  for (const auto language : {"c", "csharp", "js", "typescript", "go", "rust",
      "python", "swift", "kotlin"}) {
    SCOPED_TRACE(language);
    const auto output = schema::writer::portable::generate(next.statements, language, "Schema");
    EXPECT_FALSE(output.empty());
    EXPECT_EQ(output, schema::writer::portable::generate(unqualified, language, "Schema"));
    EXPECT_EQ(output.find("noexcept"), std::string::npos);
  }
  const auto native = parse_noexcept("serializer version 1.13.0; class sample { "
      "public function bool complete() readonly noexcept(cpp) python { return True }; }");
  EXPECT_NO_THROW(schema::writer::portable::generate(native, "python", "Schema"));
}
