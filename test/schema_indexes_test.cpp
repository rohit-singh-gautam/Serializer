// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <schema_indexes.hpp>
#include <rohit/managed.hpp>
#include <rohit/native_database.hpp>
#include <rohit/serializer_creator.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <array>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
namespace schema = rohit::serializer;
using root = indexed_example::document;
using entry = indexed_example::entry;
using store_type = managed::model_store<root>;
using traits = managed::model_traits<root>;
using descriptors = managed::fields<entry>;

// Parse and resolve a complete schema through the public compiler entry point.
auto parse(std::string_view text) {
  return schema::parser::parse(rohit::make_constant_stream(text.data(), text.size()), true);
}

// Populate private unique values through generated constructors' ordinary descriptor bindings.
root initial() {
  root value;
  value.entries.resize(2);
  value.entries[0].key = 10;
  value.entries[1].key = 20;
  value.entries[0].title = "shared";
  value.entries[1].title = "shared";
  value.entries[0].kind = indexed_example::category::fiction;
  value.entries[1].kind = indexed_example::category::reference;
  value.entries[0].available = true;
  value.entries[1].available = false;
  managed::value_fields<entry>::field_sequence().write(value.entries[0], -10);
  managed::value_fields<entry>::field_sequence().write(value.entries[1], 20);
  value.others.resize(1);
  value.others[0].key = 10;
  return value;
}

// Reject find_unique at compile time for an ordinary or nonunique field descriptor.
template <typename Descriptor>
concept unique_lookup = requires(const managed::registry_view<root>& view, Descriptor descriptor,
    typename Descriptor::value_type key) { view.find_unique(descriptor, key); };

static_assert(unique_lookup<decltype(descriptors::field_key())>);
static_assert(!unique_lookup<decltype(descriptors::field_title())>);
static_assert(!unique_lookup<decltype(descriptors::field_note())>);
static_assert(traits::has_unique_indexes);

// Build deliberately invalid legacy envelopes to prove strict import checks current and retained states.
struct unconstrained_traits : traits {
  static constexpr bool has_unique_indexes = false;
};

// Own only freshly created compiler fixtures under the operating system temporary directory.
struct include_directory {
  // Remove a trailing separator so lexical parent equality is independent of TEMP spelling.
  static std::filesystem::path temporary_base() {
    auto result = std::filesystem::temp_directory_path();
    if (result.filename().empty() && result.parent_path() != result) { result = result.parent_path(); }
    return result;
  }
  const std::filesystem::path base{temporary_base()};
  const std::filesystem::path path{base / ("serializer-indexes-" + std::to_string(managed::make_document_id().low))};
  // Refuse existing paths so cleanup cannot remove another task's files.
  include_directory() {
    if (path.parent_path() != base || !std::filesystem::create_directory(path)) {
      throw std::runtime_error{"Cannot create isolated index include fixtures"};
    }
  }
  // Remove only this fixture's verified child directory after parser handles have closed.
  ~include_directory() {
    if (path.parent_path() == base) {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
    }
  }
  // Persist one complete schema before the parser opens its dependency graph.
  void write(std::string_view name, std::string_view text) const {
    std::ofstream output{path / name, std::ios::binary};
    output << text;
    if (!output) { throw std::runtime_error{"Cannot write index include fixture"}; }
  }
};
} // namespace

// Declarations preserve payload IDs and resolve old types named index without reserving the word.
TEST(schema_indexes, syntax_version_and_legacy_type) {
  const auto nodes = parse("serializer version 1.24.0; class data managed { public index string title (2); "
      "public index(unordered, unique, on = local) uint64 key (3); }");
  const auto& record = static_cast<const schema::class_node&>(*nodes.front());
  ASSERT_EQ(record.member_list.size(), 2u);
  EXPECT_EQ(record.member_list[0].indexing, schema::member::index_method::ordered);
  EXPECT_FALSE(record.member_list[0].index_unique);
  EXPECT_EQ(record.member_list[1].indexing, schema::member::index_method::unordered);
  EXPECT_TRUE(record.member_list[1].index_unique);
  EXPECT_TRUE(record.member_list[1].index_local);
  EXPECT_EQ(record.member_list[1].id, 3u);
  EXPECT_NO_THROW(parse("serializer version 1; class index { public uint32 value (1); } "
      "class holder { public index value (1); }"));
  EXPECT_THROW(parse("serializer version 1.23.0; class data managed { public index string title (1); }"),
      rohit::exception::base_parser);
}

// Each file's declared language contract controls its features independently of include order.
TEST(schema_indexes, included_language_contract) {
  include_directory directory;
  directory.write("root.serializer", "serializer version 1.24.0; include child; class root {} ");
  directory.write("child.serializer", "serializer version 1.23.0; class child managed { public index uint64 key (1); }");
  try {
    static_cast<void>(schema::parser::parse_file(directory.path / "root.serializer"));
    FAIL() << "Included file accepted indexes under an older language contract";
  } catch (const std::invalid_argument& error) {
    const std::string_view diagnostic{error.what()};
    EXPECT_NE(diagnostic.find("child.serializer"), std::string_view::npos);
    EXPECT_NE(diagnostic.find("require serializer version 1.24.0"), std::string_view::npos);
  }
  directory.write("root.serializer", "serializer version 1; include child; class root {} ");
  directory.write("child.serializer", "serializer version 1.24.0; class child managed { public index uint64 key (1); }");
  EXPECT_NO_THROW(schema::parser::parse_file(directory.path / "root.serializer"));
}

// Unsupported capabilities and key semantics fail explicitly before generation or publication.
TEST(schema_indexes, invalid_options_and_field_contracts) {
  for (const auto declaration : {
      "index() uint64 key", "index(ordered, unordered) uint64 key", "index(unique, unique) uint64 key",
      "index(on = local, on = inherit) uint64 key", "index(on = database) uint64 key",
      "index(on = both) uint64 key", "index(method = hash) uint64 key", "index(gin) string key",
      "index(nulls = equal) string key", "index float64 key", "index optional<uint64> key",
      "index array uint64 key", "index transient uint64 key", "index cache uint64 key",
      "index uint64 key omit(json)", "obsolete index uint64 key"}) {
    SCOPED_TRACE(declaration);
    EXPECT_THROW(parse(std::string{"serializer version 1.24.0; class data managed { public "} +
        declaration + " (1); }"), rohit::exception::base_parser);
  }
  EXPECT_THROW(parse("serializer version 1.24.0; class data { public index uint64 key (1); }"),
      rohit::exception::base_parser);
  EXPECT_THROW(parse("serializer version 1.24.0; class key {} class data managed { public index key value (1); }"),
      rohit::exception::base_parser);
}

// Backend selection never silently erases native constraints or index readiness semantics.
TEST(schema_indexes, unsupported_backend_diagnostics) {
  const auto nodes = parse("serializer version 1.24.0; class data managed { public index uint64 key (1); }");
  for (const auto language : {"c", "csharp", "javascript", "typescript", "go", "rust", "python", "swift", "kotlin"}) {
    SCOPED_TRACE(language);
    EXPECT_THROW(schema::writer::portable::generate(nodes, language, "Unit"), std::invalid_argument);
  }
  EXPECT_THROW(schema::writer::java::generate(nodes, "Unit"), std::invalid_argument);
}

// Generated metadata identifies the scalar path without modifying payload storage or wire identity.
TEST(schema_indexes, stable_metadata_and_schema_identity) {
  constexpr auto key = descriptors::field_key();
  constexpr auto title = descriptors::field_title();
  static_assert(key.index().id == 1u && key.index().field_id == 1u);
  static_assert(key.index().unique && key.index().method == managed::index_method::unordered);
  static_assert(title.index().site == managed::index_site::local);
  static_assert(title.index().comparator_version == 1u);
  schema::writer::cpp_options options;
  options.format = false;
  const auto binding = [&](std::string_view modifier) {
    const auto generated = schema::writer::cpp::generate(parse(std::string{
        "serializer version 1.24.0; class data managed stable_ids { public "} +
        std::string{modifier} + "uint64 key (1); }"), options);
    constexpr std::string_view marker = "schema_id = \"";
    const auto found = generated.find(marker);
    if (found == std::string::npos) { throw std::runtime_error{"Missing managed schema identity"}; }
    const auto begin = found + marker.size();
    return generated.substr(begin, generated.find('"', begin) - begin);
  };
  EXPECT_EQ(binding(""), binding("index(unordered, unique) "));
  EXPECT_EQ(binding(""), binding("index(ordered) "));
}

// Exact-type indexed lookup returns immutable handles and documents empty-result behavior.
TEST(schema_indexes, typed_equality_lookup_and_snapshot_pins) {
  store_type store{initial()};
  const auto matches = store.registry().find_by(descriptors::field_title(), "shared");
  ASSERT_EQ(matches.size(), 2u);
  const auto pinned = store.registry().find_unique(descriptors::field_key(), 10);
  ASSERT_TRUE(pinned.has_value());
  const auto id = pinned->id();
  EXPECT_EQ(pinned->value().title, "shared");
  EXPECT_EQ(store.registry().find_by(descriptors::field_kind(), indexed_example::category::fiction).size(), 1u);
  EXPECT_EQ(store.registry().find_by(descriptors::field_available(), true).size(), 1u);
  EXPECT_EQ(store.registry().find_unique(descriptors::field_sequence(), -10)->id(), id);
  EXPECT_FALSE(store.registry().find_unique(descriptors::field_key(), 999).has_value());
  EXPECT_TRUE(store.registry().find_by(descriptors::field_title(), "absent").empty());
  EXPECT_TRUE(store.registry().contains_by(descriptors::field_key(), 10));
  EXPECT_FALSE(store.registry().contains_by(descriptors::field_key(), 999));
  store.execute_transaction([&](auto& tx) { tx.registry().template edit<entry>(id).set_title("changed"); }).throw_if_failed();
  EXPECT_EQ(pinned->value().title, "shared");
  for (const auto& match : matches) { EXPECT_EQ(match.value().title, "shared"); }
  EXPECT_EQ(store.registry().find_by(descriptors::field_title(), "changed").size(), 1u);
  store.undo();
  EXPECT_EQ(store.registry().find_by(descriptors::field_title(), "shared").size(), 2u);
  store.redo();
  EXPECT_EQ(store.registry().find_by(descriptors::field_title(), "changed").size(), 1u);
}

// Initial construction validates every owned occurrence of the exact type across sibling containers.
TEST(schema_indexes, automatic_unique_activation_and_type_scope) {
  auto duplicate = initial();
  duplicate.entries[1].key = duplicate.entries[0].key;
  EXPECT_THROW((store_type{std::move(duplicate)}), managed::registry_error);
  auto duplicate_private = initial();
  managed::value_fields<entry>::field_sequence().write(duplicate_private.entries[1], -10);
  EXPECT_THROW((store_type{std::move(duplicate_private)}), managed::registry_error);
  auto sibling = initial();
  sibling.archive.push_back(sibling.entries[0]);
  EXPECT_THROW((store_type{std::move(sibling)}), managed::registry_error);
  // The same application key on a different concrete type remains legal.
  EXPECT_NO_THROW((store_type{initial()}));
}

// Raw and typed candidate mutation share one rejection boundary and preserve accepted bytes and history.
TEST(schema_indexes, transactional_unique_rejection_is_atomic) {
  store_type store{initial()};
  const auto id = store.registry().find_unique(descriptors::field_key(), 10)->id();
  const auto before = store.save();
  const auto typed = store.execute_transaction([&](auto& tx) {
    tx.registry().template edit<entry>(id).set_key(20);
  });
  EXPECT_EQ(typed.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), before);
  const auto raw = store.execute_transaction([](auto& tx) {
    tx.update([](auto& candidate) {
      managed::model_traits<entry>::payload(traits::payload(candidate).entries[0]).key = 20;
    });
  });
  EXPECT_EQ(raw.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), before);
  store.execute_transaction([&](auto& tx) {
    tx.registry().template edit<entry>(id).set_key(30);
  }).throw_if_failed();
  EXPECT_EQ(store.registry().find_unique(descriptors::field_key(), 30)->id(), id);
  store.undo();
  EXPECT_EQ(store.registry().find_unique(descriptors::field_key(), 10)->id(), id);
  store.redo();
  EXPECT_EQ(store.registry().find_unique(descriptors::field_key(), 30)->id(), id);
}

// Index constraints remain active in the native no-history profile and after exact save/load.
TEST(schema_indexes, disabled_history_and_load) {
  managed::model_store<root, managed::history_mode::disabled> store{initial()};
  const auto before = store.save();
  const auto id = store.registry().find_unique(descriptors::field_key(), 10)->id();
  EXPECT_EQ(store.execute_transaction([&](auto& tx) {
    tx.registry().template edit<entry>(id).set_key(20);
  }).status, managed::transaction_status::failed);
  store.load(before);
  EXPECT_EQ(store.registry().find_unique(descriptors::field_key(), 10)->id(), id);
}

// Reject invalid current and historical imports atomically, even when an older writer accepted them.
TEST(schema_indexes, load_validates_current_and_retained_unique_constraints) {
  using legacy_store = managed::model_store<root, managed::history_mode::linear,
      managed::history_labels::disabled, unconstrained_traits>;
  auto duplicate = initial();
  duplicate.entries[1].key = 10;
  legacy_store old{std::move(duplicate)};
  store_type store{initial()};
  const auto before = store.save();
  EXPECT_THROW(store.load(old.save()), managed::registry_error);
  EXPECT_EQ(store.save(), before);
  const auto id = traits::payload(*old.read()).entries[0].persistent_id;
  old.execute_transaction([&](auto& tx) {
    tx.registry().template edit<entry>(id).set_key(30);
  }).throw_if_failed();
  EXPECT_TRUE(old.registry().find_unique(descriptors::field_key(), 30).has_value());
  EXPECT_THROW(store.load(old.save()), managed::registry_error);
  EXPECT_EQ(store.save(), before);
}

// Tree navigation uses the same generated invariants and source-specific query pins.
TEST(schema_indexes, tree_history_navigation) {
  managed::model_store<root, managed::history_mode::tree> store{initial()};
  const auto id = store.registry().find_unique(descriptors::field_key(), 10)->id();
  EXPECT_EQ(store.execute_transaction([&](auto& tx) {
    tx.registry().template edit<entry>(id).set_key(20);
  }).status, managed::transaction_status::failed);
  store.execute_transaction([&](auto& tx) {
    tx.registry().template edit<entry>(id).set_key(30);
  }).throw_if_failed();
  store.undo();
  EXPECT_EQ(store.registry().find_unique(descriptors::field_key(), 10)->id(), id);
}

// Concurrent readers publish one reusable index and obtain stable handles from a single snapshot.
TEST(schema_indexes, concurrent_lazy_index_queries) {
  store_type store{initial()};
  const auto view = store.registry();
  const auto query = [&] {
    for (int count = 0; count != 100; ++count) {
      const auto match = view.find_unique(descriptors::field_key(), 10);
      if (!match || match->value().title != "shared") { return false; }
    }
    return true;
  };
  auto first = std::async(std::launch::async, query);
  auto second = std::async(std::launch::async, query);
  EXPECT_TRUE(first.get());
  EXPECT_TRUE(second.get());
}

// Repeated lookup reuses one immutable access path; derived pins release their resident charges.
TEST(schema_indexes, cache_reuse_and_budget_release) {
  store_type store{initial()};
  const auto before = store.resident_bytes();
  {
    const auto view = store.registry();
    EXPECT_TRUE(view.contains_by(descriptors::field_key(), 10));
    const auto indexed_bytes = store.resident_bytes();
    EXPECT_GT(indexed_bytes, before);
    for (int query = 0; query != 10; ++query) {
      EXPECT_TRUE(view.contains_by(descriptors::field_key(), 20));
      EXPECT_EQ(store.resident_bytes(), indexed_bytes);
    }
  }
  EXPECT_EQ(store.resident_bytes(), before);
  EXPECT_THROW(store.registry({100, 1}), std::length_error);
  auto large = initial();
  large.entries[0].title.assign(8192, 'x');
  store_type large_store{std::move(large)};
  const auto limited = large_store.registry({100, 2048});
  EXPECT_THROW(limited.contains_by(descriptors::field_title(), "shared"), std::length_error);
  // Failed title construction does not poison independent declared indexes.
  EXPECT_TRUE(limited.contains_by(descriptors::field_key(), 10));
  managed::store_options options;
  options.decode.max_work_units = 1;
  EXPECT_THROW((store_type{initial(), {11, 22}, options}), std::length_error);
}

// Native storage enforces declared constraints before durability and rebuilds indexes from accepted data.
TEST(schema_indexes, native_database_constraints_and_snapshot_queries) {
  include_directory directory;
  const auto path = directory.path / "indexed.sdb";
  const std::array<std::uint8_t, 1> request{17};
  const managed::document_id document{31, 41};
  constexpr std::uint64_t epoch = 7;
  using database = managed::native_database<root>;
  std::optional<database::snapshot_type> pinned;
  {
    database state{path, initial(), document, epoch};
    pinned.emplace(state.snapshot());
    EXPECT_EQ(pinned->document(), document);
    EXPECT_EQ(pinned->epoch(), epoch);
    EXPECT_EQ(pinned->revision(), 0u);
    const auto old = state.registry();
    const auto id = old.find_unique(descriptors::field_key(), 10)->id();
    EXPECT_THROW(state.prepare({1, 1}, epoch, 0, request, [&](auto& tx) {
      tx.registry().template edit<entry>(id).set_key(20);
    }), managed::registry_error);
    EXPECT_EQ(state.revision(), 0u);
    auto prepared = state.prepare({1, 1}, epoch, 0, request, [&](auto& tx) {
      tx.registry().template edit<entry>(id).set_key(30);
    });
    EXPECT_TRUE(state.registry().find_unique(descriptors::field_key(), 10));
    EXPECT_EQ(state.accept(std::move(prepared)).revision, 1u);
    EXPECT_EQ(state.registry().find_unique(descriptors::field_key(), 30)->id(), id);
    EXPECT_TRUE(old.find_unique(descriptors::field_key(), 10));
    EXPECT_FALSE(old.find_unique(descriptors::field_key(), 30));
    EXPECT_EQ(state.snapshot().revision(), 1u);
  }
  EXPECT_EQ(pinned->revision(), 0u);
  EXPECT_TRUE(pinned->registry().find_unique(descriptors::field_key(), 10));
  EXPECT_FALSE(pinned->registry().find_unique(descriptors::field_key(), 30));
  database reopened{path};
  EXPECT_TRUE(reopened.registry().find_unique(descriptors::field_key(), 30));
  EXPECT_FALSE(reopened.registry().find_unique(descriptors::field_key(), 10));
  auto retry = reopened.prepare({1, 1}, epoch, 0, request, [](auto&) {
    throw std::runtime_error{"Exact retry must not edit again"};
  });
  EXPECT_EQ(reopened.accept(std::move(retry)).revision, 1u);
  EXPECT_EQ(reopened.registry().find_by(descriptors::field_title(), "shared").size(), 2u);
}
