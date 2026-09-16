#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>
#include <storage_reuse.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;
constexpr std::size_t old_string_bytes = 256;
constexpr std::size_t incoming_string_bytes = 96;

template <codec::serialize_type Direction>
using big_none = codec::binary<Direction, codec::serialize_key_type::none, std::endian::big>;
template <codec::serialize_type Direction>
using big_integer = codec::binary<Direction, codec::serialize_key_type::integer, std::endian::big>;
template <codec::serialize_type Direction>
using big_string = codec::binary<Direction, codec::serialize_key_type::string, std::endian::big>;

// Read one exact JSON value with a fresh budget, retaining the caller's destination storage.
template <typename T>
void read_json(std::string_view text, T& value, codec::decode_limits limits = {}) {
  const auto input = rohit::make_constant_full_stream(text.data(), text.size());
  codec::json<codec::serialize_type::in> decoder{input, limits};
  decoder.serialize_in(value);
  decoder.finish();
}

// Decode independently owned bytes into an existing destination, optionally using a sparse peer.
template <template <codec::serialize_type> class Protocol, typename Source, typename Destination>
void round_trip_into(const Source& source, Destination& destination) {
  rohit::full_stream_auto_alloc output{};
  Protocol<codec::serialize_type::out> encoder{output};
  encoder.serialize_out(source);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  Protocol<codec::serialize_type::in> decoder{input};
  decoder.serialize_in(destination);
  decoder.finish();
}

// Build nested owning storage, including inherited fields, object arrays, and maps.
storage_reuse::record make_record(std::size_t text_bytes, char character) {
  storage_reuse::record result{};
  result.inherited.assign(text_bytes, character);
  result.child.label.assign(text_bytes, character);
  result.child.numbers = {1, 2, 3};
  result.child.revision = 42;
  result.children.push_back(result.child);
  result.entries.emplace(1, result.child);
  result.labels.emplace(1, result.child.label);
  result.payload.number = 17;
  result.status = storage_reuse::phase::active;
  return result;
}

// Repeated decoding must retain nested buffers and nodes without retaining old logical values.
template <template <codec::serialize_type> class Protocol>
void check_generated_reuse() {
  const std::vector<storage_reuse::record> source{make_record(incoming_string_bytes, 'n')};
  std::vector<storage_reuse::record> decoded{make_record(old_string_bytes, 'o'),
                                             make_record(old_string_bytes, 'x')};
  const auto* outer = decoded.data();
  const auto* inherited = decoded[0].inherited.data();
  const auto* child_label = decoded[0].child.label.data();
  const auto* child_numbers = decoded[0].child.numbers.data();
  const auto* children = decoded[0].children.data();
  const auto* element_label = decoded[0].children[0].label.data();
  const auto* entry = &*decoded[0].entries.begin();
  const auto* entry_label = entry->second.label.data();
  const auto* label_node = &*decoded[0].labels.begin();
  const auto* label = label_node->second.data();
  for (int attempt = 0; attempt < 2; ++attempt) {
    round_trip_into<Protocol>(source, decoded);
    ASSERT_EQ(decoded.size(), source.size());
    EXPECT_EQ(decoded.data(), outer);
    EXPECT_EQ(decoded[0].inherited, source[0].inherited);
    EXPECT_EQ(decoded[0].inherited.data(), inherited);
    EXPECT_EQ(decoded[0].child.label, source[0].child.label);
    EXPECT_EQ(decoded[0].child.label.data(), child_label);
    EXPECT_EQ(decoded[0].child.numbers, source[0].child.numbers);
    EXPECT_EQ(decoded[0].child.numbers.data(), child_numbers);
    EXPECT_EQ(decoded[0].child.revision, source[0].child.revision);
    ASSERT_EQ(decoded[0].children.size(), 1u);
    EXPECT_EQ(decoded[0].children.data(), children);
    EXPECT_EQ(decoded[0].children[0].label.data(), element_label);
    EXPECT_EQ(decoded[0].children[0].label, source[0].child.label);
    ASSERT_EQ(decoded[0].entries.size(), 1u);
    EXPECT_EQ(&*decoded[0].entries.begin(), entry);
    EXPECT_EQ(decoded[0].entries.at(1).label.data(), entry_label);
    EXPECT_EQ(decoded[0].entries.at(1).label, source[0].child.label);
    ASSERT_EQ(decoded[0].labels.size(), 1u);
    EXPECT_EQ(&*decoded[0].labels.begin(), label_node);
    EXPECT_EQ(decoded[0].labels.at(1).data(), label);
    EXPECT_EQ(decoded[0].labels.at(1), source[0].child.label);
    EXPECT_EQ(decoded[0].payload_type, storage_reuse::record::e_payload::number);
    EXPECT_EQ(decoded[0].payload.number, 17u);
    EXPECT_EQ(decoded[0].status, storage_reuse::phase::active);
  }
}

// Reuse direct nested containers without requiring generated object support.
template <template <codec::serialize_type> class Protocol>
void check_container_reuse() {
  using rows = std::vector<std::vector<std::string>>;
  rows decoded{{std::string(old_string_bytes, 'o'), "discard"}, {"discard row"}};
  const auto* outer = decoded.data();
  const auto* row = decoded[0].data();
  const auto* text = decoded[0][0].data();
  rows source{{std::string(incoming_string_bytes, 'n')}};
  round_trip_into<Protocol>(source, decoded);
  ASSERT_EQ(decoded, source);
  EXPECT_EQ(decoded.data(), outer);
  EXPECT_EQ(decoded[0].data(), row);
  EXPECT_EQ(decoded[0][0].data(), text);
  source.resize(decoded.capacity() + 1, source.front());
  round_trip_into<Protocol>(source, decoded);
  EXPECT_EQ(decoded, source);
  const auto capacity = decoded.capacity();
  source.clear();
  round_trip_into<Protocol>(source, decoded);
  EXPECT_TRUE(decoded.empty());
  EXPECT_EQ(decoded.capacity(), capacity);

  std::map<std::uint32_t, std::string> labels{{1, std::string(old_string_bytes, 'o')}};
  const auto* node = &*labels.begin();
  const auto* buffer = node->second.data();
  const std::map<std::uint32_t, std::string> replacement{
      {2, std::string(incoming_string_bytes, 'n')}};
  round_trip_into<Protocol>(replacement, labels);
  ASSERT_EQ(labels, replacement);
  EXPECT_EQ(&*labels.begin(), node);
  EXPECT_EQ(labels.at(2).data(), buffer);
  round_trip_into<Protocol>(std::map<std::uint32_t, std::string>{}, labels);
  EXPECT_TRUE(labels.empty());
}

// Keyed collection entries start from schema defaults even when an old object donates storage.
template <template <codec::serialize_type> class Protocol>
void check_missing_fields() {
  storage_reuse::sparse_item sparse{};
  sparse.label.assign(incoming_string_bytes, 'n');
  storage_reuse::item previous{};
  previous.label.assign(old_string_bytes, 'o');
  previous.numbers = {88};
  previous.revision = 99;
  std::vector<storage_reuse::item> decoded{previous};
  const auto* buffer = decoded[0].label.data();
  round_trip_into<Protocol>(std::vector<storage_reuse::sparse_item>{sparse}, decoded);
  ASSERT_EQ(decoded.size(), 1u);
  EXPECT_EQ(decoded[0].label, sparse.label);
  EXPECT_EQ(decoded[0].label.data(), buffer);
  EXPECT_TRUE(decoded[0].numbers.empty());
  EXPECT_EQ(decoded[0].revision, 7u);

  std::map<std::uint32_t, storage_reuse::item> mapped{{1, previous}};
  round_trip_into<Protocol>(std::map<std::uint32_t, storage_reuse::sparse_item>{{1, sparse}},
                            mapped);
  EXPECT_TRUE(mapped.at(1).numbers.empty());
  EXPECT_EQ(mapped.at(1).revision, 7u);

  // Direct object updates retain omitted fields as before.
  round_trip_into<Protocol>(sparse, previous);
  EXPECT_EQ(previous.numbers, (std::vector<std::uint32_t>{88}));
  EXPECT_EQ(previous.revision, 99u);
}

// A truncated variable-width binary element commits only the preceding complete prefix.
template <template <codec::serialize_type> class Protocol>
void check_binary_partial_replacement() {
  rohit::full_stream_auto_alloc output{};
  Protocol<codec::serialize_type::out> encoder{output};
  encoder.serialize_out(std::vector<std::string>{"complete", "truncated"});
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset() - 1);
  Protocol<codec::serialize_type::in> decoder{input};
  std::vector<std::string> destination(3, std::string(old_string_bytes, 'o'));
  EXPECT_THROW(decoder.serialize_in(destination), codec::exception::bad_input_data);
  EXPECT_EQ(destination, (std::vector<std::string>{"complete"}));
}

// Every native protocol uses the same storage rules; binary covers both wire byte orders.
template <template <codec::serialize_type> class Protocol>
void check_protocol() {
  check_generated_reuse<Protocol>();
  check_container_reuse<Protocol>();
  if constexpr (Protocol<codec::serialize_type::in>::key_type != codec::serialize_key_type::none) {
    check_missing_fields<Protocol>();
  }
  if constexpr (requires { Protocol<codec::serialize_type::in>::wire_endian; }) {
    check_binary_partial_replacement<Protocol>();
  }
}

// A custom reader without generated storage support must still receive a fresh candidate.
struct custom_value {
  std::uint32_t value{7};
  // Read the payload while checking the construction state supplied by the collection decoder.
  void serialize_in(auto& protocol) {
    EXPECT_EQ(value, 7u);
    protocol.serialize_in(value);
  }
};

// Map insertion supports constructible keys even when node rekeying cannot assign them.
struct nonassignable_key {
  std::uint32_t value{};
  // Create the fresh key temporary needed by map input.
  nonassignable_key() = default;
  // Initialize an old map key for replacement tests.
  explicit nonassignable_key(std::uint32_t initial) : value{initial} {}
  // Permit constructing a map node from the decoded key.
  nonassignable_key(const nonassignable_key&) = default;
  // Deliberately disable the node-reuse optimization for this key type.
  nonassignable_key& operator=(const nonassignable_key&) = delete;
  // Order the immutable keys by their decoded numeric value.
  bool operator<(const nonassignable_key& other) const {
    return value < other.value;
  }
  // Decode a numeric key without requiring key assignment.
  void serialize_in(auto& protocol) {
    protocol.serialize_in(value);
  }
};
} // namespace

// Exercise native formats, generated parents/unions, shrinking/growing arrays, and map rekeying.
TEST(nested_storage_reuse, native_protocols) {
  check_protocol<codec::json>();
  check_protocol<codec::binary_none>();
  check_protocol<codec::binary_integer>();
  check_protocol<codec::binary_string>();
  check_protocol<big_none>();
  check_protocol<big_integer>();
  check_protocol<big_string>();
}

// Repeated object fields merge within a fresh entry; absent fields never borrow old logical values.
TEST(nested_storage_reuse, keyed_defaults_and_repeated_fields) {
  std::vector<storage_reuse::record> decoded{make_record(old_string_bytes, 'o')};
  read_json(R"([{"child":{"label":"first"},"child":{"revision":9},"children":[{}]}])", decoded);
  ASSERT_EQ(decoded.size(), 1u);
  EXPECT_TRUE(decoded[0].inherited.empty());
  EXPECT_EQ(decoded[0].child.label, "first");
  EXPECT_EQ(decoded[0].child.revision, 9u);
  EXPECT_TRUE(decoded[0].child.numbers.empty());
  ASSERT_EQ(decoded[0].children.size(), 1u);
  EXPECT_EQ(decoded[0].children[0].label, "schema default");
  EXPECT_EQ(decoded[0].children[0].revision, 7u);
  EXPECT_TRUE(decoded[0].entries.empty());
  EXPECT_EQ(decoded[0].status, storage_reuse::phase::idle);
}

// A malformed later element removes unused old slots and keeps only complete incoming values.
TEST(nested_storage_reuse, json_partial_replacement) {
  std::vector<std::vector<std::string>> decoded{{"old"}, {"old"}, {"old"}};
  EXPECT_THROW(read_json(R"([["complete"],["pending",false]])", decoded),
               rohit::exception::base_parser);
  EXPECT_EQ(decoded, (std::vector<std::vector<std::string>>{{"complete"}}));

  std::map<std::uint32_t, std::string> mapped{{8, "old"}, {9, "old"}, {10, "old"}};
  EXPECT_THROW(read_json(R"([{"key":1,"value":"complete"},{"key":1,"value":"pending",}])", mapped),
               rohit::exception::base_parser);
  EXPECT_EQ(mapped, (std::map<std::uint32_t, std::string>{{1, "complete"}}));
  read_json(R"([{"key":1,"value":"first"},{"key":1,"value":"last"}])", mapped);
  EXPECT_EQ(mapped, (std::map<std::uint32_t, std::string>{{1, "last"}}));

  std::vector<custom_value> custom{{99}, {88}};
  read_json("[1]", custom);
  ASSERT_EQ(custom.size(), 1u);
  EXPECT_EQ(custom[0].value, 1u);
}

// Binary maps never donate from a complete incoming duplicate when a later value is truncated.
TEST(nested_storage_reuse, binary_duplicate_failure) {
  rohit::full_stream_auto_alloc output{};
  codec::binary_none<codec::serialize_type::out> encoder{output};
  encoder.serialize_out_variable(3);
  encoder.serialize_out(std::uint32_t{1});
  encoder.serialize_out(std::string{"first"});
  encoder.serialize_out(std::uint32_t{1});
  encoder.serialize_out(std::string{"complete"});
  encoder.serialize_out(std::uint32_t{1});
  encoder.serialize_out_variable(10); // Missing string bytes.
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  codec::binary_none<codec::serialize_type::in> decoder{input};
  std::map<std::uint32_t, std::string> value{{8, "old"}, {9, "old"}, {10, "old"}};
  EXPECT_THROW(decoder.serialize_in(value), codec::exception::bad_input_data);
  EXPECT_EQ(value, (std::map<std::uint32_t, std::string>{{1, "complete"}}));
}

// Custom nonassignable keys retain the original clear-and-insert map path.
TEST(nested_storage_reuse, nonassignable_map_keys) {
  std::map<nonassignable_key, std::string> value;
  value.emplace(nonassignable_key{9}, "old");
  read_json(R"([{"key":1,"value":"first"},{"key":1,"value":"last"}])", value);
  ASSERT_EQ(value.size(), 1u);
  EXPECT_EQ(value.begin()->first.value, 1u);
  EXPECT_EQ(value.begin()->second, "last");
}

// Reusing physical capacity must not bypass logical allocation or collection limits.
TEST(nested_storage_reuse, limits_apply_to_retained_storage) {
  codec::decode_limits limits{};
  constexpr std::string_view json = R"(["first","second"] )";
  std::vector<std::string> fresh;
  fresh.reserve(2);
  std::vector<std::string> reused{std::string(old_string_bytes, 'o'),
                                  std::string(old_string_bytes, 'o')};
  limits.max_allocation_bytes = 2 * sizeof(std::string) + std::string_view{"first"}.size();
  EXPECT_THROW(read_json(json, fresh, limits), codec::exception::resource_limit);
  EXPECT_THROW(read_json(json, reused, limits), codec::exception::resource_limit);
  EXPECT_EQ(reused, fresh);
  EXPECT_EQ(reused, (std::vector<std::string>{"first"}));
  limits = {};
  limits.max_collection_elements = 1;
  EXPECT_THROW(read_json(json, reused, limits), codec::exception::resource_limit);
  EXPECT_EQ(reused, (std::vector<std::string>{"first"}));
}

// Generated donor dispatch must consume exactly the same work and input as a fresh destination.
TEST(nested_storage_reuse, work_boundaries_match_fresh_decoding) {
  constexpr std::string_view json = R"([{"label":"one"},{"label":"two"}])";
  bool reached_success = false;
  constexpr std::size_t budget_ceiling = 8 * json.size();
  for (std::size_t budget = 0; budget < budget_ceiling; ++budget) {
    SCOPED_TRACE(budget);
    codec::decode_limits limits{};
    limits.max_work_units = budget;
    std::vector<storage_reuse::item> fresh;
    std::vector<storage_reuse::item> reused(2);
    for (auto& item : reused) {
      item.label.assign(old_string_bytes, 'o');
      item.revision = 99;
    }
    const auto fresh_input = rohit::make_constant_full_stream(json.data(), json.size());
    const auto reused_input = rohit::make_constant_full_stream(json.data(), json.size());
    codec::json<codec::serialize_type::in> fresh_decoder{fresh_input, limits};
    codec::json<codec::serialize_type::in> reused_decoder{reused_input, limits};
    bool fresh_failed = false;
    bool reused_failed = false;
    try {
      fresh_decoder.serialize_in(fresh);
      fresh_decoder.finish();
    } catch (const codec::exception::resource_limit&) {
      fresh_failed = true;
    }
    try {
      reused_decoder.serialize_in(reused);
      reused_decoder.finish();
    } catch (const codec::exception::resource_limit&) {
      reused_failed = true;
    }
    EXPECT_EQ(reused_failed, fresh_failed);
    EXPECT_EQ(reused_input.current_offset(), fresh_input.current_offset());
    // Before the collection's opening token commits, its old destination is intentionally intact.
    if (fresh_input.current_offset() != 0) {
      ASSERT_EQ(reused.size(), fresh.size());
      for (std::size_t index = 0; index < fresh.size(); ++index) {
        EXPECT_EQ(reused[index].label, fresh[index].label);
        EXPECT_EQ(reused[index].revision, fresh[index].revision);
      }
    }
    if (!fresh_failed) {
      reached_success = true;
      break;
    }
  }
  EXPECT_TRUE(reached_success);
}
