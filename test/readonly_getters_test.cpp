// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <readonly_getters.hpp>
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace readonly_getters {
// The existing native friendship contract writes the original backing fields without setters.
struct field_access {
  // Mutate durable state and a transient cache through authorized friendship.
  static void write(record& owner, std::int32_t number, std::uint32_t cache) {
    owner.number_ = number;
    owner.cache_ = cache;
    owner.payload_.value_ = number;
  }
  // Locate private storage directly to qualify that generated getters introduce no copy.
  static const std::int32_t* number_address(const record& owner) {
    return std::addressof(owner.number_);
  }
  // Locate the exact private string object rather than its internal character allocation.
  static const std::string* label_address(const record& owner) {
    return std::addressof(owner.label_);
  }
  // Read an accessor whose declaration remains private to friends and its owner.
  static const std::int32_t& private_value(const access_probe& owner) {
    return owner.private_value();
  }
  // Mirror durable values into the pre-exposure layout to compare every native wire encoding.
  static baseline_record baseline(const record& owner) {
    baseline_record result;
    result.number_ = owner.number_;
    result.label_ = owner.label_;
    result.coordinates_ = owner.coordinates_;
    result.payload_.value_ = owner.payload_.value_;
    result.cache_ = owner.cache_;
    return result;
  }
};
} // namespace readonly_getters

namespace {
namespace schema = rohit::serializer;
using record = readonly_getters::record;

// A borrowed accessor is callable only when this expression has a live lvalue receiver.
template <typename Owner>
concept has_number = requires(Owner&& owner) { std::forward<Owner>(owner).number(); };
// Mutation through the exposure must fail for both mutable and const owners.
template <typename Owner>
concept writable_number = requires(Owner& owner) { owner.number() = std::int32_t{9}; };
// No mutation helper is synthesized along with a readonly exposure.
template <typename Owner>
concept has_number_setter = requires(Owner& owner) { owner.set_number(std::int32_t{9}); };
// Getter access follows its declared C++ visibility rather than the visibility of backing storage.
template <typename Owner>
concept has_protected_value = requires(const Owner& owner) { owner.protected_value(); };
template <typename Owner>
concept has_private_value = requires(const Owner& owner) { owner.private_value(); };

static_assert(has_number<record&> && has_number<const record&>);
static_assert(!has_number<record> && !has_number<const record>);
static_assert(!writable_number<record> && !writable_number<const record>);
static_assert(!has_number_setter<record>);
static_assert(std::is_same_v<decltype(std::declval<record&>().number()), const std::int32_t&>);
static_assert(!std::is_convertible_v<decltype(std::declval<record&>().number()), std::int32_t&>);
static_assert(std::is_same_v<decltype(std::declval<const record&>().label()), const std::string&>);
static_assert(std::is_same_v<decltype(std::declval<const record&>().coordinates()),
                             const std::array<std::int32_t, 2>&>);
static_assert(std::is_same_v<decltype(std::declval<const readonly_getters::box<record>&>().value()),
                             const record&>);
static_assert(std::is_same_v<decltype(std::declval<const readonly_getters::integer_box&>().value()),
                             const std::int32_t&>);
static_assert(noexcept(std::declval<const record&>().number()));
static_assert(!has_protected_value<readonly_getters::access_probe>);
static_assert(!has_private_value<readonly_getters::access_probe>);
static_assert(sizeof(readonly_getters::integer_box) == sizeof(std::int32_t));
static_assert(sizeof(record) == sizeof(readonly_getters::baseline_record));

// The generated direct reference accessor remains usable in constant expressions.
constexpr bool constant_getter() {
  readonly_getters::integer_box owner;
  return owner.value() == 0;
}
static_assert(constant_getter());

// Read an exposed protected member from a normal derived native receiver.
struct derived_access : readonly_getters::access_probe {
  // The protected declaration gives derived owners access without making it public.
  const std::int32_t& inspect() const { return protected_value(); }
};

// Compile one source without formatting so tests can inspect exact generated declarations.
auto parse(std::string_view source) {
  return schema::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}

// Encode the complete durable payload through each unchanged native Serializer protocol.
template <template <schema::serialize_type> typename Protocol, typename Value>
std::string encode(const Value& value) {
  rohit::full_stream_auto_alloc bytes;
  value.template serialize_out<Protocol>(bytes);
  return {reinterpret_cast<const char*>(bytes.begin()), bytes.current_offset()};
}

// Decode exact bytes into fresh field defaults, including omitted transient cache values.
template <template <schema::serialize_type> typename Protocol>
void round_trip(const record& owner) {
  const auto bytes = encode<Protocol>(owner);
  EXPECT_EQ(encode<Protocol>(readonly_getters::field_access::baseline(owner)), bytes);
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  const auto decoded = schema::deserialize_exact<record, Protocol>(input);
  EXPECT_EQ(decoded.number(), owner.number());
  EXPECT_EQ(decoded.label(), owner.label());
  EXPECT_EQ(decoded.coordinates(), owner.coordinates());
  EXPECT_EQ(decoded.payload().value(), owner.payload().value());
  EXPECT_EQ(decoded.cache(), 5u);
  EXPECT_EQ(encode<Protocol>(decoded), bytes);
}
} // namespace

// Friends write the backing object; every returned address and type remains exactly that field.
TEST(readonly_getters, backing_identity_generic_types_and_visibility) {
  record owner;
  EXPECT_EQ(owner.number(), 7);
  EXPECT_EQ(owner.label(), "seed");
  EXPECT_EQ(std::addressof(owner.number()), readonly_getters::field_access::number_address(owner));
  EXPECT_EQ(std::addressof(owner.label()), readonly_getters::field_access::label_address(owner));
  const auto& borrowed = owner.number();
  readonly_getters::field_access::write(owner, 42, 19);
  EXPECT_EQ(borrowed, 42);
  EXPECT_EQ(owner.payload().value(), 42);
  EXPECT_EQ(owner.cache(), 19u);
  derived_access protected_owner;
  EXPECT_EQ(protected_owner.inspect(), 0);
  readonly_getters::access_probe private_owner;
  EXPECT_EQ(readonly_getters::field_access::private_value(private_owner), 0);
}

// Getters add no field identities, change no durable bytes and preserve fresh-decode transient defaults.
TEST(readonly_getters, unchanged_protocols_and_transient_contract) {
  record owner;
  readonly_getters::field_access::write(owner, 29, 101);
  round_trip<schema::json>(owner);
  round_trip<schema::binary_none>(owner);
  round_trip<schema::binary_integer>(owner);
  round_trip<schema::binary_string>(owner);
  EXPECT_EQ(encode<schema::json>(owner),
            "{\"number\":29,\"label\":\"seed\",\"coordinates\":[4,9],\"payload\":{\"value\":29}}");
}

// An ordinary record inside managed state retains pins, undo/redo and transient reset boundaries.
TEST(readonly_getters, ordinary_record_in_managed_history) {
  using root = readonly_getters::history_root;
  using traits = rohit::managed::model_traits<root>;
  rohit::managed::model_store<root> store{root{}};
  const auto pinned = store.read();
  const auto before = encode<schema::binary_integer>(traits::payload(*pinned).snapshot);
  store.execute_transaction([](auto& tx) {
    tx.update([](auto& candidate) {
      readonly_getters::field_access::write(traits::payload(candidate).snapshot, 67, 91);
    });
  }).throw_if_failed();
  EXPECT_EQ(traits::payload(*store.read()).snapshot.number(), 67);
  EXPECT_EQ(traits::payload(*store.read()).snapshot.cache(), 5u);
  EXPECT_EQ(traits::payload(*pinned).snapshot.number(), 7);
  store.undo();
  EXPECT_EQ(traits::payload(*store.read()).snapshot.number(), 7);
  EXPECT_EQ(encode<schema::binary_integer>(traits::payload(*store.read()).snapshot), before);
  store.redo();
  EXPECT_EQ(traits::payload(*store.read()).snapshot.number(), 67);
}

// The new contract requires its own language header, one explicit name, and valid readonly access.
TEST(readonly_getters, language_gate_and_malformed_exposure) {
  for (const auto declaration : {
      "private int32 value expose readonly(public);",
      "private int32 value expose readonly(public) as;",
      "private int32 value expose mutable(public) as read;",
      "private int32 value expose readonly() as read;",
      "private int32 value expose readonly(internal) as read;",
      "private int32 value expose readonly(public) as read expose readonly(public) as again;",
      "private version uint32 (1) {1} expose readonly(public) as read;",
      "private wire_only uint32 size (1) auto_size(binary_none) expose readonly(public) as read;"}) {
    const std::string source = "serializer version 1.14.0; class owner { " +
        std::string{declaration} + " }";
    EXPECT_THROW(parse(source), rohit::exception::base_parser) << declaration;
  }
  EXPECT_THROW(parse("serializer version 1.13.0; class owner { private int32 value expose readonly(public) as read; }"),
               rohit::exception::base_parser);
  EXPECT_NO_THROW(parse("serializer version 1.14.0; class owner { private int32 value expose readonly(public) as read; }"));
}

// Reject source collisions and unsupported storage/backends instead of dropping declared exposure.
TEST(readonly_getters, source_collisions_and_unsupported_profiles) {
  for (const auto body : {
      "private int32 value expose readonly(public) as value;",
      "private int32 first expose readonly(public) as read; private int32 second expose readonly(public) as read;",
      "private int32 first expose readonly(public) as read; public int32 read;",
      "private int32 first expose readonly(public) as read; public function int32 read() readonly expression(first);",
      "private int32 first expose readonly(public) as owner;",
      "reserve variable { read }; private int32 first expose readonly(public) as read;"}) {
    const std::string source = "serializer version 1.14.0; class owner { " +
        std::string{body} + " }";
    EXPECT_THROW(parse(source), rohit::exception::base_parser) << body;
  }
  for (const auto modifiers : {"view readonly", "owning view readonly", "managed", "packed"}) {
    const std::string source = "serializer version 1.14.0; class owner " +
        std::string{modifiers} + " { private int32 value expose readonly(public) as read; }";
    EXPECT_THROW(parse(source), rohit::exception::base_parser) << modifiers;
  }
  const auto statements = parse("serializer version 1.14.0; class owner<T> { private T stored expose readonly(public) as read; }");
  for (const auto language : {"javascript", "typescript", "csharp", "go", "rust", "python", "swift", "kotlin", "c"}) {
    EXPECT_THROW(schema::writer::portable::generate(statements, language, "fixture"), std::invalid_argument);
  }
  EXPECT_THROW(schema::writer::java::generate(statements, "fixture"), std::invalid_argument);
}

// Naming transforms check accessors alongside fields, methods and runtime codec helpers.
TEST(readonly_getters, generated_name_collision_and_metadata) {
  const auto statements = parse("serializer version 1.14.0; class owner { private int32 value_ (\"wire\", 7) expose readonly(public) as value; }");
  const auto& object = static_cast<const schema::class_node&>(*statements.front());
  ASSERT_EQ(object.member_list.size(), 1u);
  EXPECT_EQ(object.member_list.front().id, 7u);
  EXPECT_EQ(object.member_list.front().display_name, "wire");
  EXPECT_EQ(object.member_list.front().access, schema::access_type::private_access);
  schema::writer::cpp_options options;
  options.format = false;
  options.rename_identifiers = false;
  const auto generated = schema::writer::cpp::generate(statements, options);
  EXPECT_NE(generated.find("constexpr inline const ::std::int32_t& value() const & noexcept"), std::string::npos);
  EXPECT_NE(generated.find("value() const && = delete"), std::string::npos);
  const auto colliding = parse("serializer version 1.14.0; class owner { private int32 stored expose readonly(public) as serialize_out; }");
  EXPECT_THROW(schema::writer::cpp::generate(colliding, options), std::invalid_argument);
  const auto folded = parse("serializer version 1.14.0; class owner { private int32 stored expose readonly(public) as readValue; public int32 read_value; }");
  options.rename_identifiers = true;
  EXPECT_THROW(schema::writer::cpp::generate(folded, options), std::invalid_argument);
}
