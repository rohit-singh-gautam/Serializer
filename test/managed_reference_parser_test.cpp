// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
namespace serializer = rohit::serializer;

// Parse through the public compilation-unit path, including deferred generic binding.
auto parse_references(std::string_view source) {
  return serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()), true);
}

// Find original or lowered classes without depending on namespace emission blocks.
const serializer::class_node* find_record(
    const std::vector<std::unique_ptr<serializer::syntax_node>>& nodes, std::string_view name) {
  for (const auto& node : nodes) {
    if (node->type == serializer::object_type::namespace_type) {
      if (const auto* found = find_record(static_cast<const serializer::namespace_node&>(*node).statements, name)) {
        return found;
      }
    } else if ((node->type == serializer::object_type::class_type ||
                node->type == serializer::object_type::generic_definition) && node->get_full_name() == name) {
      return static_cast<const serializer::class_node*>(node.get());
    }
  }
  return nullptr;
}

// Reserve a test-owned include directory whose destructor never adopts an existing path.
class reference_directory {
  std::filesystem::path root_{};
public:
  // Create a unique temporary input directory or fail before writing fixtures.
  reference_directory() {
    static std::atomic<unsigned> next{};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
        ("serializer-managed-reference-" + std::to_string(tick) + "-" + std::to_string(next++));
    if (!std::filesystem::create_directory(root_)) { throw std::runtime_error{"Cannot reserve reference test directory"}; }
  }
  // Remove only the directory exclusively reserved by this fixture.
  ~reference_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(root_, ignored);
  }
  // Write one local included schema and return its absolute path.
  std::filesystem::path write(std::string_view name, std::string_view contents) const {
    const auto path = root_ / name;
    std::ofstream output{path, std::ios::binary};
    output << contents;
    if (!output) { throw std::runtime_error{"Cannot write reference test schema"}; }
    return path;
  }
};
} // namespace

// Reciprocal and self tokens bind identities while preserving field IDs and optional presence.
TEST(managed_reference_parser, forward_self_and_container_targets) {
  const auto nodes = parse_references(R"(serializer version 1.20.0;
    namespace graph {
      class first managed class_id(18446744073709551615) {
        public managed_ref<second> next (7);
        public optional<managed_ref<first>> parent (8);
        public array managed_ref<second> items (9);
        public map(uint32) managed_ref<second> lookup (10);
      }
      class second managed { public managed_ref<first> previous (11); }
    })");
  const auto* first = find_record(nodes, "graph::first");
  const auto* second = find_record(nodes, "graph::second");
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first->explicit_class_id, std::numeric_limits<std::uint64_t>::max());
  const auto& next = first->member_list[0].type_name_list.front();
  ASSERT_TRUE(next.is_managed_reference());
  ASSERT_EQ(next.arguments.size(), 1U);
  EXPECT_EQ(next.arguments.front().resolved_node, second);
  EXPECT_EQ(next.resolved_node, nullptr);
  EXPECT_EQ(first->member_list[0].id, 7U);
  const auto& parent = first->member_list[1].type_name_list.front();
  ASSERT_TRUE(parent.is_optional());
  ASSERT_TRUE(parent.arguments.front().is_managed_reference());
  EXPECT_EQ(parent.arguments.front().arguments.front().resolved_node, first);
  EXPECT_TRUE(first->member_list[2].type_name_list.front().is_managed_reference());
  EXPECT_TRUE(first->member_list[3].type_name_list.front().is_managed_reference());
  EXPECT_EQ(second->member_list.front().type_name_list.front().arguments.front().resolved_node, first);
}

// Concrete substitution retains explicit class identity without turning tokens into owning edges.
TEST(managed_reference_parser, generic_targets_and_class_identity_copies) {
  auto nodes = parse_references(R"(serializer version 1.20.0;
    class token_box<T> managed class_id (99) { public managed_ref<T> target (4); }
    class item managed { public int32 value (1); }
    instantiate item_box = token_box<item>;
    class holder { public managed_ref<token_box<item>> box (5); })");
  const auto* box = find_record(nodes, "item_box");
  const auto* item = find_record(nodes, "item");
  const auto* holder = find_record(nodes, "holder");
  ASSERT_NE(box, nullptr);
  ASSERT_NE(item, nullptr);
  ASSERT_NE(holder, nullptr);
  EXPECT_EQ(box->explicit_class_id, 99U);
  EXPECT_EQ(box->named_generic_source, "token_box");
  ASSERT_EQ(box->named_generic_arguments.size(), 1U);
  EXPECT_EQ(box->named_generic_arguments.front().resolved_node, item);
  EXPECT_EQ(box->member_list.front().type_name_list.front().arguments.front().resolved_node, item);
  const auto& concrete = holder->member_list.front().type_name_list.front().arguments.front();
  ASSERT_NE(concrete.resolved_node, nullptr);
  EXPECT_EQ(static_cast<const serializer::class_node&>(*concrete.resolved_node).explicit_class_id, 99U);
}

// Recursive specializations and dimension-dependent target applications carry no inline ownership.
TEST(managed_reference_parser, recursive_generic_and_dimension_targets) {
  const auto nodes = parse_references(R"(serializer version 1.20.0;
    class node<T> managed { public managed_ref<node<T>> next; }
    instantiate integer_node = node<int32>;
    class payload<uint64 N> managed { public array[N] uint32 values; }
    class holder<uint64 M> { public managed_ref<payload<M + 1>> target; }
    instantiate sized_holder = holder<2>;)");
  const auto* node = find_record(nodes, "integer_node");
  const auto* holder = find_record(nodes, "sized_holder");
  ASSERT_NE(node, nullptr);
  ASSERT_NE(holder, nullptr);
  const auto* concrete = node->member_list.front().type_name_list.front().arguments.front().resolved_node;
  ASSERT_NE(concrete, nullptr);
  const auto& self = static_cast<const serializer::class_node&>(*concrete);
  EXPECT_EQ(self.member_list.front().type_name_list.front().arguments.front().resolved_node, concrete);
  const auto* target = holder->member_list.front().type_name_list.front().arguments.front().resolved_node;
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(static_cast<const serializer::class_node&>(*target).member_list.front().fixed_extent, 3U);
}

// Phantom target deferral does not relax existing source-order rules for owned values.
TEST(managed_reference_parser, ordinary_forward_ownership_stays_rejected) {
  EXPECT_ANY_THROW(parse_references(R"(serializer version 1.20.0;
    class first { public managed_ref<second> token; public second owned; }
    class second managed { public int32 value; })"));
}

// Tokens require an eligible concrete target and cannot manufacture identities from defaults.
TEST(managed_reference_parser, invalid_targets_and_storage_are_rejected) {
  for (const auto source : {
      "class owner { public managed_ref<uint32> token; }",
      "class owner { public managed_ref<missing> token; }",
      "class plain {} class owner { public managed_ref<plain> token; }",
      "class plain managed packed {} class owner { public managed_ref<plain> token; }",
      "class plain managed view {} class owner { public managed_ref<plain> token; }",
      "class plain managed {} class owner packed { public managed_ref<plain> token; }",
      "class plain managed {} class owner { public managed_ref<plain> token {0}; }",
      "class plain managed {} class owner { public managed_ref<plain,plain> token; }",
      "class plain managed {} class owner { public managed_ref<> token; }"}) {
    EXPECT_ANY_THROW(parse_references(std::string{"serializer version 1.20.0; "} + source)) << source;
  }
}

// Explicit class IDs preserve the full unsigned range and refuse narrowing or ambiguous metadata.
TEST(managed_reference_parser, explicit_class_id_is_checked_uint64) {
  for (const auto attribute : {"class_id(0)", "class_id(-1)", "class_id(18446744073709551616)",
                               "class_id(1.0)", "class_id(1) class_id(2)", "class_id()"}) {
    EXPECT_ANY_THROW(parse_references(std::string{"serializer version 1.20.0; class item managed "} +
        attribute + " {}")) << attribute;
  }
  const auto nodes = parse_references("serializer version 1.20.0; class item managed class_id(42) {}");
  const auto* item = find_record(nodes, "item");
  ASSERT_NE(item, nullptr);
  serializer::class_node moved{std::move(*static_cast<serializer::class_node*>(nodes.front().get()))};
  EXPECT_EQ(moved.explicit_class_id, 42U);
}

// Per-file language provenance survives later binding and a newer entry schema.
TEST(managed_reference_parser, older_contracts_and_includes_reject_new_features) {
  EXPECT_ANY_THROW(parse_references("serializer version 1.19.0; class item managed class_id(2) {}"));
  EXPECT_ANY_THROW(parse_references("serializer version 1.19.0; class item managed { public managed_ref<item> token; }"));
  reference_directory directory;
  directory.write("older.serializer", "serializer version 1.19.0; class item managed { public managed_ref<item> token; }");
  const auto root = directory.write("root.serializer", "serializer version 1.20.0; include older;");
  EXPECT_ANY_THROW(serializer::parser::parse_file(root));
}

// The new built-in does not capture a previously valid user-declared generic type.
TEST(managed_reference_parser, older_user_type_spelling_remains_valid) {
  const auto nodes = parse_references(R"(serializer version 1.19.0;
    class managed_ref<T> { public T value; }
    class owner { public managed_ref<int32> value; })");
  const auto* owner = find_record(nodes, "owner");
  ASSERT_NE(owner, nullptr);
  EXPECT_FALSE(owner->member_list.front().type_name_list.front().is_managed_reference());
  EXPECT_EQ(owner->member_list.front().type_name_list.front().type, serializer::object_type::class_type);
}

// Unsupported wire generators receive an explicit token diagnostic rather than an owning surrogate.
TEST(managed_reference_parser, non_cpp_guard_rejects_nested_tokens) {
  const auto nodes = parse_references(R"(serializer version 1.20.0;
    class owner { public optional<managed_ref<item>> token; }
    class item managed {})");
  EXPECT_THROW(serializer::require_unmanaged_backend(nodes), std::invalid_argument);
}
