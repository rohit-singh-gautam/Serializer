// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cache_attributes.hpp>
#include <rohit/cache_codec.hpp>
#include <rohit/file_stream.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_journal_document.hpp>
#include <rohit/managed_journal_stream.hpp>
#include <rohit/serializer_creator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

namespace {
namespace cache = rohit::cache;
namespace managed = rohit::managed;
namespace schema = rohit::serializer;
using root = cache_example::document;
using store = managed::model_store<root>;

// Each test owns one disposable directory below the CTest build working directory.
class cache_test : public testing::Test {
protected:
  std::filesystem::path directory{};
  // Create a unique build-local namespace without touching application files.
  void SetUp() override {
    const auto identity = managed::make_document_id();
    directory = std::filesystem::current_path() / ("cache-test-" + std::to_string(identity.high));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
  }
  // All store/provider handles have closed before deleting this verified test-owned path.
  void TearDown() override {
    ASSERT_EQ(directory.parent_path(), std::filesystem::current_path());
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
  }
  // Remove the free-space reserve only for these small deterministic fixtures.
  std::shared_ptr<cache::directory_provider> provider() const {
    cache::directory_options options;
    options.reserve_free_bytes = 0;
    return std::make_shared<cache::directory_provider>(directory / "cache", options);
  }
};

// Parse complete strings through numeric language gating and the real semantic resolver.
auto parse(std::string_view text) {
  return schema::parser::parse(rohit::make_constant_stream(text.data(), text.size()), true);
}

// Omitted priority is exactly zero; cache declarations do not consume implicit wire identities.
TEST(cache_schema, priority_identity_and_legacy_identifier) {
  const auto nodes = parse("serializer version 1.22.0; class data { public uint32 first; "
                           "public cache uint64 cached; public cache(priority = 255) string "
                           "expensive; public uint32 last; }");
  const auto& record = static_cast<const schema::class_node&>(*nodes.front());
  ASSERT_EQ(record.member_list.size(), 4u);
  EXPECT_TRUE(record.member_list[1].cached);
  EXPECT_EQ(record.member_list[1].cache_priority, 0u);
  EXPECT_EQ(record.member_list[2].cache_priority, 255u);
  EXPECT_EQ(record.member_list[3].id, record.member_list[0].id + 1);
  EXPECT_NO_THROW(parse("serializer version 1; class cache { public uint32 value; } "
                        "class use { public cache ordinary (1); }"));
  schema::writer::cpp_options options;
  options.format = false;
  // Compare the actual generated binding with and without disposable declarations.
  const auto binding = [&](std::string_view fields) {
    const auto generated = schema::writer::cpp::generate(
        parse(std::string{"serializer version 1.22.0; class document managed stable_ids { "
                          "public string body (1); "} +
              std::string{fields} + " }"),
        options);
    constexpr std::string_view marker = "schema_id = \"";
    const auto found = generated.find(marker);
    if (found == std::string::npos) {
      throw std::runtime_error{"Missing generated schema identity"};
    }
    const auto begin = found + marker.size();
    return generated.substr(begin, generated.find('"', begin) - begin);
  };
  EXPECT_EQ(binding(""), binding("public cache uint64 count {0};"));
  EXPECT_EQ(binding(""), binding("public cache(priority = 255) uint64 count {17};"));
}

// Invalid priorities, old declaring headers and durable field metadata fail before generation.
TEST(cache_schema, reject_invalid_declarations) {
  for (const auto text :
       {"cache(priority = -1) uint32 value", "cache(priority = 256) uint32 value",
        "cache(priority = 999999999999999999999) uint32 value",
        "cache(priority = 1.5) uint32 value", "cache(priority = 1, priority = 2) uint32 value",
        "cache(other = 1) uint32 value", "cache transient uint32 value", "cache uint32 value (1)",
        "cache uint32 value omit(json)"}) {
    SCOPED_TRACE(text);
    EXPECT_THROW(
        parse(std::string{"serializer version 1.22.0; class data { public "} + text + "; }"),
        rohit::exception::base_parser);
  }
  EXPECT_THROW(parse("serializer version 1.21.0; class data { public cache uint32 value; }"),
               rohit::exception::base_parser);
}

// Outputs without a native cache contract reject explicitly, rather than silently persisting fields.
TEST(cache_schema, backend_diagnostics) {
  const auto nodes = parse("serializer version 1.22.0; class data { public cache uint32 value; }");
  for (const auto language : {"c", "csharp", "java", "javascript", "typescript", "go", "rust",
                              "python", "swift", "kotlin"}) {
    SCOPED_TRACE(language);
    if (std::string_view{language} == "java") {
      EXPECT_THROW(schema::writer::java::generate(nodes, "Unit"), std::invalid_argument);
    } else {
      EXPECT_THROW(schema::writer::portable::generate(nodes, language, "Unit"),
                   std::invalid_argument);
    }
  }
}

// Generated private/owning bindings preserve defaults, empty hits and ordinary document bytes.
TEST(cache_schema, generated_bindings_and_codecs) {
  cache_example::payload value;
  const auto initial = cache::encode(value);
  const auto count = cache::fields<cache_example::payload>::count();
  EXPECT_EQ(count.metadata.priority, 0u);
  count.assign(value, 17);
  EXPECT_EQ(count.read(value), 17u);
  EXPECT_EQ(initial, cache::encode(value));
  EXPECT_EQ(cache::decode<std::uint64_t>(cache::encode(std::uint64_t{})), 0u);
  const auto preview = cache::fields<cache_example::payload>::preview();
  cache_example::derived_value derived;
  derived.text = "Preview";
  derived.bins = {1, 2, 3};
  preview.assign(value, derived);
  const auto restored =
      cache::decode<cache_example::derived_value>(cache::encode(preview.read(value)));
  EXPECT_EQ(restored.text, "Preview");
  EXPECT_EQ(restored.bins, derived.bins);
  const auto selection = cache::fields<cache_example::payload>::selection();
  auto variant = selection.read(value);
  variant.template emplace<1>(99);
  EXPECT_EQ(std::get<1>(cache::decode<decltype(variant)>(cache::encode(variant))), 99u);
  const auto buckets = cache::fields<cache_example::payload>::buckets();
  buckets.assign(value, {1, 2, 3});
  EXPECT_EQ(cache::decode<std::vector<std::uint32_t>>(cache::encode(buckets.read(value))),
            buckets.read(value));
  const auto totals = cache::fields<cache_example::payload>::totals();
  totals.assign(value, {{"a", 7}, {"b", 9}});
  using total_type = std::map<std::string, std::uint64_t>;
  EXPECT_EQ(cache::decode<total_type>(cache::encode(totals.read(value))), totals.read(value));
}

// The generated schema default reaches the runtime automatically; policy changes preserve identity.
TEST_F(cache_test, declared_priority_and_override) {
  store model{root{}};
  const auto key = model.cache_key(cache::fields<root>::preview().metadata, "preview.v1");
  EXPECT_EQ(key.declared_priority, 200u);
  cache::service service{provider()};
  unsigned rebuilt{};
  const auto build = [&] {
    ++rebuilt;
    return cache::encode(std::string{"preview"});
  };
  auto first = service.get_or_rebuild(key, build);
  ASSERT_EQ(first.state, cache::status::ready);
  EXPECT_EQ(first.pin->priority, 200u);
  cache::request_options options;
  options.priority = 0;
  const auto second = service.get_or_rebuild(key, build, options);
  ASSERT_EQ(second.state, cache::status::ready);
  EXPECT_EQ(second.pin->priority, 0u);
  EXPECT_EQ(first.pin->identity, second.pin->identity);
  EXPECT_EQ(rebuilt, 1u);
}

// Explicit purge survives reopening even while an old independent owning result remains pinned.
TEST_F(cache_test, invalidation_and_domain_exclusion) {
  store model{root{}};
  const auto key = model.cache_key({"purge", 0}, "v1");
  std::shared_ptr<const cache::entry> old;
  {
    auto storage = provider();
    storage->publish(key, cache::bytes{7});
    old = storage->lookup(key);
    ASSERT_TRUE(old);
    EXPECT_THROW((void)provider(), std::system_error);
    EXPECT_TRUE(storage->invalidate(key));
    EXPECT_FALSE(storage->lookup(key));
  }
  EXPECT_FALSE(provider()->lookup(key));
  EXPECT_EQ(old->value, (cache::bytes{7}));
}

// Rebuild/output/admission failures remain optional and cannot alter authoritative bytes.
TEST_F(cache_test, bounds_and_optional_write_failure) {
  store model{root{}};
  const auto initial = model.save();
  const auto key = model.cache_key({"bounds", 0}, "v1");
  cache::service memory;
  cache::request_options options;
  options.max_value_bytes = 2;
  EXPECT_EQ(memory.get_or_rebuild(
                      key, [] { return cache::bytes{1, 2, 3}; }, options)
                .state,
            cache::status::failed);
  cache::service limited{{}, 1, 1};
  EXPECT_EQ(limited.get_or_rebuild(key, [] { return cache::bytes{}; }).state,
            cache::status::failed);
  cache::directory_options disk_options;
  disk_options.reserve_free_bytes = std::numeric_limits<std::uint64_t>::max();
  auto disk = std::make_shared<cache::directory_provider>(directory / "reserved", disk_options);
  cache::service optional{disk};
  const auto ready = optional.get_or_rebuild(key, [] { return cache::bytes{9}; });
  EXPECT_EQ(ready.state, cache::status::ready);
  EXPECT_TRUE(ready.storage_error);
  EXPECT_EQ(model.save(), initial);
}

// Atomic record selection preserves complete old/new entries at every cache write boundary.
TEST_F(cache_test, cache_publication_faults) {
  store model{root{}};
  const auto key = model.cache_key({"fault", 20}, "v1");
  for (const auto point : {cache::io_event::before_stage, cache::io_event::after_stage,
                           cache::io_event::before_publish, cache::io_event::after_publish}) {
    bool armed{};
    cache::directory_options options;
    options.reserve_free_bytes = 0;
    options.fault_injector = [&](cache::io_event actual) {
      if (armed && actual == point) {
        throw std::runtime_error{"cache interrupted"};
      }
    };
    const auto folder = directory / ("fault-" + std::to_string(static_cast<int>(point)));
    {
      cache::directory_provider disk{folder, options};
      disk.publish(key, cache::bytes{1});
      armed = true;
      EXPECT_THROW(disk.publish(key, cache::bytes{2}), std::runtime_error);
    }
    options.fault_injector = {};
    cache::directory_provider reopened{folder, options};
    const auto selected = reopened.lookup(key);
    ASSERT_TRUE(selected);
    EXPECT_EQ(
        selected->value,
        (cache::bytes{static_cast<std::uint8_t>(point == cache::io_event::after_publish ? 2 : 1)}));
  }
}

// Reopening optional storage reuses only a complete exact key, independent of default-valued bytes.
TEST_F(cache_test, persistent_reuse_recipe_and_corruption) {
  store model{root{}};
  const auto key = model.cache_key(cache::fields<root>::count().metadata, "count.v1");
  std::atomic<unsigned> builds{};
  const auto rebuild = [&] {
    ++builds;
    return cache::encode(std::uint64_t{});
  };
  {
    cache::service service{provider()};
    const auto hit = service.get_or_rebuild(key, rebuild);
    ASSERT_EQ(hit.state, cache::status::ready);
    ASSERT_FALSE(hit.storage_error);
    EXPECT_EQ(cache::decode<std::uint64_t>(hit.pin->value), 0u);
  }
  {
    cache::service service{provider()};
    ASSERT_EQ(service.get_or_rebuild(key, rebuild).state, cache::status::ready);
    EXPECT_EQ(builds, 1u);
    auto changed = key;
    changed.recipe[0] ^= 1;
    ASSERT_EQ(service.get_or_rebuild(changed, rebuild).state, cache::status::ready);
    EXPECT_EQ(builds, 2u);
  }
  for (const auto& item : std::filesystem::directory_iterator(directory / "cache")) {
    if (item.path().extension() == ".srcache") {
      std::ofstream{item.path(), std::ios::binary | std::ios::trunc} << "broken";
    }
  }
  cache::service service{provider()};
  ASSERT_EQ(service.get_or_rebuild(key, rebuild).state, cache::status::ready);
  EXPECT_EQ(builds, 3u);
}

// Priority affects eligible disk entries; a live reader pin can postpone lower-priority deletion.
TEST_F(cache_test, priority_eviction_and_pins) {
  std::vector<std::set<std::uint8_t>> before_removal;
  const auto folder = directory / "cache";
  cache::directory_options options;
  options.reserve_free_bytes = 0;
  options.fault_injector = [&](cache::io_event event) {
    if (event != cache::io_event::before_remove) {
      return;
    }
    // Observe independent public record priority bytes before each deletion, without calling
    // back into a locked provider or depending on unrelated filesystem free-space changes.
    std::set<std::uint8_t> remaining;
    for (const auto& item : std::filesystem::directory_iterator(folder)) {
      if (item.path().extension() != ".srcache") {
        continue;
      }
      std::ifstream input{item.path(), std::ios::binary};
      input.seekg(2 * 8);
      const auto priority = input.get();
      if (priority == std::char_traits<char>::eof()) {
        throw std::runtime_error{"Missing fixture priority"};
      }
      remaining.insert(static_cast<std::uint8_t>(priority));
    }
    before_removal.push_back(std::move(remaining));
  };
  auto storage = std::make_shared<cache::directory_provider>(folder, options);
  store model{root{}};
  auto low = model.cache_key({"low", 0}, "v1");
  auto high = model.cache_key({"high", 255}, "v1");
  storage->publish(low, cache::bytes(8192, 1), 0);
  storage->publish(high, cache::bytes(8192, 2), 255);
  {
    auto pin = storage->lookup(low);
    ASSERT_TRUE(pin);
    const auto reclaimed = storage->reclaim(std::numeric_limits<std::uint64_t>::max());
    EXPECT_GT(reclaimed.pinned_bytes, 0u);
    EXPECT_FALSE(storage->lookup(high));
    EXPECT_EQ(pin->value.front(), 1u);
  }
  before_removal.clear();
  storage->publish(high, cache::bytes(8192, 2), 255);
  const auto reclaimed = storage->reclaim(std::numeric_limits<std::uint64_t>::max());
  EXPECT_EQ(reclaimed.removed_entries, 2u);
  ASSERT_EQ(before_removal.size(), 2u);
  EXPECT_EQ(before_removal[1], (std::set<std::uint8_t>{255}));
  EXPECT_FALSE(storage->lookup(low));
  EXPECT_FALSE(storage->lookup(high));
}

// Concurrent callers coalesce work, and failed/canceled work never becomes a ready entry.
TEST_F(cache_test, coalescing_readiness_and_failure) {
  store model{root{}};
  const auto key = model.cache_key({"coalesced", 0}, "v1");
  // Revalidation of retained values must work with an entirely in-memory service.
  cache::service memory;
  ASSERT_EQ(memory.get_or_rebuild(key, [] { return cache::bytes{1}; }).state, cache::status::ready);
  cache::request_options invalid;
  invalid.priority = 10;
  invalid.validate = [](auto) { return false; };
  EXPECT_EQ(memory.get_or_rebuild(
                      key, [] { return cache::bytes{2}; }, invalid)
                .state,
            cache::status::failed);
  cache::service service;
  std::promise<void> started;
  std::promise<void> finish;
  auto release = finish.get_future().share();
  std::atomic<unsigned> builds{};
  auto first = std::async(std::launch::async, [&] {
    return service.get_or_rebuild(key, [&] {
      ++builds;
      started.set_value();
      release.wait();
      return cache::bytes{1};
    });
  });
  started.get_future().wait();
  EXPECT_EQ(service.state(key), cache::status::building);
  auto second = std::async(std::launch::async, [&] {
    return service.get_or_rebuild(key, [&] {
      ++builds;
      return cache::bytes{2};
    });
  });
  finish.set_value();
  const auto one = first.get();
  const auto two = second.get();
  EXPECT_EQ(builds, 1u);
  EXPECT_EQ(one.pin, two.pin);
  service.invalidate(key);
  cache::request_options options;
  options.source_is_current = [] { return false; };
  EXPECT_EQ(service.get_or_rebuild(
                       key, [] { return cache::bytes{3}; }, options)
                .state,
            cache::status::failed);
  EXPECT_EQ(service.state(key), cache::status::failed);
}

// Guarded cache publication changes neither durable bytes nor journal/history state; stale keys fail.
TEST_F(cache_test, managed_publication_and_invalidation) {
  store model{root{}};
  const auto before = model.save();
  const auto key = model.cache_key(cache::fields<root>::count().metadata, "v1");
  const auto original = model.read();
  model.publish_cache(key, [](auto& edit) { edit.root().set_count(42); }).throw_if_failed();
  EXPECT_EQ(model.save(), before);
  EXPECT_NE(original, model.read());
  model.execute_transaction([](auto& edit) { edit.root().set_body("Changed"); }).throw_if_failed();
  EXPECT_EQ(model.publish_cache(key, [](auto& edit) { edit.root().set_count(99); }).status,
            managed::transaction_status::failed);
  model.undo();
  EXPECT_EQ(model.cache_key(cache::fields<root>::count().metadata, "v1"), key);
}

// Native close retains exact identities/history and caches, with no selected recovery sidecar.
TEST_F(cache_test, native_close_retains_cache_and_history) {
  for (const auto mode :
       {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    const auto path =
        directory / (mode == managed::journal_storage_mode::appended ? "appended" : "sidecar");
    cache::bytes saved;
    {
      store model{root{}};
      model.create_journal(path, mode);
      const auto cache_dir = cache::companion_directory(path) / "cache";
      cache::directory_options options;
      options.reserve_free_bytes = 0;
      cache::directory_provider storage{cache_dir, options};
      storage.publish(model.cache_key({"preview", 20}, "v1"), cache::bytes{1}, 20);
      model.execute_transaction([](auto& edit) { edit.root().set_body("saved"); })
          .throw_if_failed();
      saved = model.save();
      model.close_journal();
      EXPECT_EQ(storage.enumerate().size(), 1u);
      EXPECT_FALSE(std::filesystem::exists(cache::companion_directory(path) / "journal"));
    }
    store reopened{root{}};
    reopened.recover_journal(path);
    EXPECT_EQ(reopened.save(), saved);
    reopened.undo();
    reopened.redo();
    EXPECT_EQ(reopened.save(), saved);
    reopened.close_journal();
  }
}

// A failed pre-publication close retains the active journal and accepted edits for recovery.
TEST_F(cache_test, failed_close_retains_recovery) {
  const auto path = directory / "document";
  managed::journal_options options;
  options.fault_injector = [](managed::journal_io_event point) {
    if (point == managed::journal_io_event::before_retire) {
      throw std::runtime_error{"close denied"};
    }
  };
  cache::bytes saved;
  {
    store model{root{}};
    model.create_journal(path, managed::journal_storage_mode::sidecar, options);
    model.execute_transaction([](auto& edit) { edit.root().set_body("recover"); })
        .throw_if_failed();
    saved = model.save();
    EXPECT_THROW(model.close_journal(), std::runtime_error);
  }
  store reopened{root{}};
  reopened.recover_journal(path);
  EXPECT_EQ(reopened.save(), saved);
}

// A data-only document retains identities/history/provenance in state/, independently of caches.
TEST_F(cache_test, external_close_and_reopen) {
  const auto path = directory / "external";
  managed::journal_options options;
  options.document_codec = managed::make_journal_document_codec<root>(
      "body.v1",
      [](const root& value) -> cache::bytes { return {value.body.begin(), value.body.end()}; },
      [](std::span<const std::uint8_t> value) {
        root result;
        result.body.assign(value.begin(), value.end());
        return result;
      });
  options.origin = {4, 5, 6};
  cache::bytes saved;
  {
    root value;
    value.body = "base";
    store model{value};
    model.create_journal(path, managed::journal_storage_mode::sidecar, options);
    model.execute_transaction([](auto& edit) { edit.root().set_body("external saved"); })
        .throw_if_failed();
    saved = model.save();
    model.close_journal();
    EXPECT_FALSE(std::filesystem::exists(cache::companion_directory(path) / "journal"));
    EXPECT_TRUE(std::filesystem::exists(cache::companion_directory(path) / "state" / "manifest"));
  }
  store reopened{root{}};
  reopened.recover_journal(path, options);
  EXPECT_EQ(reopened.save(), saved);
  EXPECT_EQ(reopened.journal_origin(), options.origin);
  reopened.undo();
  reopened.redo();
  EXPECT_EQ(reopened.save(), saved);
  reopened.close_journal();
}

// Produce released native container bytes without relying on a previous compiler binary.
void set_legacy_version(const std::filesystem::path& path, std::uint64_t version) {
  std::ifstream input{path, std::ios::binary};
  cache::bytes value{std::istreambuf_iterator<char>{input}, {}};
  input.close();
  constexpr std::size_t header_bytes = 72;
  if (value.size() < header_bytes) {
    throw std::runtime_error{"Invalid legacy fixture"};
  }
  cache::bytes encoded;
  managed::detail::put_word(encoded, version);
  std::copy(encoded.begin(), encoded.end(), value.begin() + 8);
  encoded.clear();
  managed::detail::put_word(
      encoded,
      managed::detail::checksum(std::span<const std::uint8_t>{value}.first(header_bytes - 8)));
  std::copy(encoded.begin(), encoded.end(), value.begin() + header_bytes - 8);
  std::ofstream output{path, std::ios::binary | std::ios::trunc};
  output.write(reinterpret_cast<const char*>(value.data()),
               static_cast<std::streamsize>(value.size()));
}

// Existing layouts keep working until explicit migration; both lock identities remain excluded.
TEST_F(cache_test, legacy_directory_migration) {
  for (const bool flat : {false, true}) {
    for (const auto mode :
         {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
      const auto path =
          directory / (std::string{flat ? "flat-" : "directory-"} +
                       (mode == managed::journal_storage_mode::appended ? "legacy-appended"
                                                                        : "legacy-sidecar"));
      cache::bytes saved;
      {
        store model{root{}};
        model.create_journal(path, mode);
        model.execute_transaction([](auto& edit) { edit.root().set_body("legacy"); })
            .throw_if_failed();
        model.save_journal();
        saved = model.save();
      }
      const auto shared = cache::companion_directory(path);
      const auto old = directory / ("." + path.filename().string() + ".journal");
      set_legacy_version(path, mode == managed::journal_storage_mode::appended ? 2 : 3);
      if (mode == managed::journal_storage_mode::sidecar) {
        for (const auto& item : std::filesystem::directory_iterator(shared / "journal")) {
          set_legacy_version(item.path(), 3);
          std::filesystem::rename(item.path(), shared / item.path().filename());
        }
      }
      ASSERT_EQ(shared.parent_path(), directory);
      if (!flat) {
        ASSERT_TRUE(std::filesystem::create_directory(old));
      }
      for (const auto& item : std::filesystem::directory_iterator(shared)) {
        if (item.is_regular_file()) {
          auto destination = old / item.path().filename();
          if (flat) {
            destination = path;
            const auto name = item.path().filename().string();
            destination += name == "writer.lock" ? ".lock" : ".journal" + name.substr(7);
          }
          std::filesystem::copy_file(item.path(), destination);
        }
      }
      std::filesystem::remove_all(shared);
      // Unexplained recovery namespaces must reject before selecting either layout.
      ASSERT_TRUE(std::filesystem::create_directories(shared / "journal"));
      {
        store conflicting{root{}};
        EXPECT_THROW(conflicting.recover_journal(path), std::invalid_argument);
      }
      ASSERT_EQ(shared.parent_path(), directory);
      std::filesystem::remove_all(shared);
      {
        store model{root{}};
        model.recover_journal(path);
        EXPECT_EQ(model.save(), saved);
        model.migrate_journal_layout();
        auto old_lock = flat ? std::filesystem::path{path.string() + ".lock"} : old / "writer.lock";
        EXPECT_THROW((void)rohit::file_stream(old_lock, rohit::file_open_mode::lock),
                     std::system_error);
        EXPECT_EQ(model.save(), saved);
        model.close_journal();
      }
      store reopened{root{}};
      reopened.recover_journal(path);
      EXPECT_EQ(reopened.save(), saved);
      reopened.close_journal();
    }
  }
}

// Every selected state remains recoverable when close is interrupted around durable publication.
TEST_F(cache_test, close_publication_faults) {
  for (const auto event :
       {managed::journal_io_event::before_retire, managed::journal_io_event::after_sidecar_flush,
        managed::journal_io_event::after_base_flush, managed::journal_io_event::before_replace,
        managed::journal_io_event::after_replace, managed::journal_io_event::after_publish,
        managed::journal_io_event::before_cleanup, managed::journal_io_event::after_retire}) {
    const auto path = directory / ("fault-" + std::to_string(static_cast<int>(event)));
    bool armed{};
    managed::journal_options options;
    options.fault_injector = [&](managed::journal_io_event point) {
      if (armed && point == event) {
        throw std::runtime_error{"close interruption"};
      }
    };
    cache::bytes saved;
    {
      store model{root{}};
      model.create_journal(path, managed::journal_storage_mode::sidecar, options);
      model.execute_transaction([](auto& edit) { edit.root().set_body("must survive"); })
          .throw_if_failed();
      saved = model.save();
      armed = true;
      try {
        model.close_journal();
      } catch (const std::exception&) {
      }
    }
    store reopened{root{}};
    EXPECT_NO_THROW(reopened.recover_journal(path));
    EXPECT_EQ(reopened.save(), saved);
    reopened.close_journal();
  }
}

} // namespace
