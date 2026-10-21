// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include "managed_owned.hpp"
#include "serializer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rohit::managed {
template <typename Root> struct model_traits;

// A resolver must choose one complete side; arbitrary resolved edits use a later transaction.
enum class merge_resolution { reject, target, source };
enum class merge_conflict_kind { value, membership, delete_edit };

// Conflict bytes use the ordinary keyed native codec, with explicit absence for map entries.
struct merge_conflict {
  std::uint64_t entity{};
  std::vector<std::uint32_t> scopes{};
  std::uint32_t field{};
  merge_conflict_kind kind{};
  std::vector<std::uint8_t> base{};
  std::vector<std::uint8_t> target{};
  std::vector<std::uint8_t> source{};
  bool base_present{true};
  bool target_present{true};
  bool source_present{true};
  std::vector<std::uint8_t> map_key{};
};

// Preserve the first unresolved conflict for inspection without publishing a partial candidate.
class merge_conflict_error : public std::runtime_error {
public:
  merge_conflict conflict;
  // Own the diagnostic values independently of temporary merge candidates.
  explicit merge_conflict_error(merge_conflict value)
      : std::runtime_error{"Managed branch merge conflict"}, conflict{std::move(value)} {}
};

struct merge_options {
  // Cumulative encoded comparison bytes bound work; this is not a process heap ceiling.
  std::size_t max_work_bytes{512U * serializer::decode_limits::mebibyte};
  std::function<merge_resolution(const merge_conflict&)> resolve{};
};

namespace detail {

// Apply a typed three-way merge to private storage through existing generated field traversal.
class branch_merger {
  const merge_options& options_;
  std::size_t max_value_bytes_;
  std::size_t remaining_;
  std::vector<std::uint32_t> scopes_{};

  // Every comparison spends the same ledger, including nested ownership and resolver diagnostics.
  std::vector<std::uint8_t> encode(const auto& value) {
    const auto maximum = std::min(max_value_bytes_, remaining_);
    if (maximum == 0) { throw std::length_error{"Managed merge work budget exceeded"}; }
    const stream_limits limits{std::min(stream_limits{}.min_read_buffer_bytes, maximum), maximum};
    full_stream_auto_alloc_limits output{&limits};
    serializer::binary_integer<serializer::serialize_type::out, full_stream_auto_alloc_limits>
        encoder{output};
    if constexpr (requires { value.valueless_by_exception(); value.index(); }) {
      if (value.valueless_by_exception()) { throw std::invalid_argument{"Valueless managed variant"}; }
      encoder.serialize_out_variable(value.index());
      std::visit([&](const auto& active) { encoder.serialize_out(active); }, value);
    } else { encoder.struct_serialize_out(value); }
    remaining_ -= output.current_offset();
    return {output.begin(), output.begin() + output.current_offset()};
  }

  // Equality follows durable codec values, excluding transient caches and host object padding.
  bool same(const auto& left, const auto& right) { return encode(left) == encode(right); }

  // Missing entries remain distinct from a present value with an empty encoding.
  merge_resolution conflict(std::uint64_t entity, std::uint32_t field,
                            merge_conflict_kind kind, const auto* base,
                            const auto* target, const auto* source,
                            std::vector<std::uint8_t> map_key = {}) {
    merge_conflict value;
    value.entity = entity;
    value.scopes = scopes_;
    value.field = field;
    value.kind = kind;
    value.map_key = std::move(map_key);
    value.base_present = base != nullptr;
    value.target_present = target != nullptr;
    value.source_present = source != nullptr;
    if (base) { value.base = encode(*base); }
    if (target) { value.target = encode(*target); }
    if (source) { value.source = encode(*source); }
    const auto resolution = options_.resolve ? options_.resolve(value) : merge_resolution::reject;
    if (resolution != merge_resolution::target && resolution != merge_resolution::source) {
      throw merge_conflict_error{std::move(value)};
    }
    return resolution;
  }

  // Child fields are entity-local; ordinary-base scopes from their owner must not leak into them.
  template <typename ChildTraits, typename Child>
  void child(Child& target, const Child& base, const Child& source) {
    auto parent = std::move(scopes_);
    scopes_.clear();
    try { ChildTraits::merge_collaboration(target, base, source, *this); }
    catch (...) { scopes_ = std::move(parent); throw; }
    scopes_ = std::move(parent);
  }

  // Identified maps merge independent keys and recurse only when all three entries name one entity.
  template <typename ChildTraits, typename Map>
  void map(std::uint64_t entity, std::uint32_t field, Map& target, const Map& base,
           const Map& source) {
    using key_type = typename Map::key_type;
    using child_type = typename Map::mapped_type;
    std::set<key_type, typename Map::key_compare> keys{target.key_comp()};
    for (const auto& [key, entry] : base) { static_cast<void>(entry); keys.insert(key); }
    for (const auto& [key, entry] : source) { static_cast<void>(entry); keys.insert(key); }
    for (const auto& key : keys) {
      const auto old = base.find(key), incoming = source.find(key);
      auto live = target.find(key);
      const child_type* b = old == base.end() ? nullptr : &old->second;
      const child_type* s = incoming == source.end() ? nullptr : &incoming->second;
      const child_type* t = live == target.end() ? nullptr : &live->second;
      if ((!b && !s) || (b && s && same(*b, *s))) { continue; }
      if ((!t && !s) || (t && s && same(*t, *s))) { continue; }
      if ((!b && !t) || (b && t && same(*b, *t))) {
        if (s) { target.insert_or_assign(key, *s); } else { target.erase(key); }
      } else if (b && t && s && b->persistent_id == t->persistent_id &&
                 b->persistent_id == s->persistent_id) {
        child<ChildTraits>(live->second, *b, *s);
      } else if (conflict(entity, field, !t || !s ? merge_conflict_kind::delete_edit
                                                 : merge_conflict_kind::membership,
                          b, t, s, encode(key)) == merge_resolution::source) {
        if (s) { target.insert_or_assign(key, *s); } else { target.erase(key); }
      }
    }
  }

public:
  // The caller owns options for the synchronous traversal; no aliases escape the merge operation.
  branch_merger(const merge_options& options, std::size_t max_value_bytes)
      : options_{options}, max_value_bytes_{max_value_bytes}, remaining_{options.max_work_bytes} {}

  // Coalesce equal changes, preserve target-only edits, and resolve divergent ordinary values atomically.
  void value(std::uint64_t entity, std::uint32_t field, auto& target, const auto& base,
             const auto& source) {
    if (same(base, source) || same(target, source)) { return; }
    if (same(target, base) || conflict(entity, field, merge_conflict_kind::value,
                                     &base, &target, &source) == merge_resolution::source) {
      target = source;
    }
  }

  // Preserve the complete stable scope in conflict diagnostics for ordinary inherited fields.
  template <typename Callable>
  void with_path(std::uint32_t base, Callable&& callback) {
    scopes_.push_back(base);
    try { std::forward<Callable>(callback)(*this); }
    catch (...) { scopes_.pop_back(); throw; }
    scopes_.pop_back();
  }

  // Merge fields of one surviving identified alternative; switches retain atomic replacement semantics.
  void owned_variant(std::uint64_t entity, std::uint32_t field, auto& target,
                     const auto& base, const auto& source) {
    if (visit_matching_owned_variant(target, base, source,
        [&](auto& live, const auto& old, const auto& incoming) {
          using child_type = std::remove_cvref_t<decltype(live)>;
          child<model_traits<child_type>>(live, old, incoming);
        })) { return; }
    value(entity, field, target, base, source);
  }

  // Lists use atomic membership; unchanged membership permits independent identified child edits.
  template <typename ChildTraits, typename Value>
  void owned(std::uint64_t entity, std::uint32_t field, Value& target, const Value& base,
             const Value& source) {
    if constexpr (requires { typename Value::key_type; typename Value::mapped_type; }) {
      if constexpr (requires(typename Value::mapped_type entry) { entry.persistent_id; }) {
        map<ChildTraits>(entity, field, target, base, source);
      } else {
        // Nested wrappers require a separate key/shape policy; retain whole-field conflict semantics.
        value(entity, field, target, base, source);
      }
    } else {
      if (same(base, source) || same(target, source)) { return; }
      if (same(target, base)) { target = source; return; }
      const auto b = owned_membership(base), t = owned_membership(target), s = owned_membership(source);
      if (b != t || b != s) {
        if (conflict(entity, field, merge_conflict_kind::membership, &base, &target, &source) ==
            merge_resolution::source) { target = source; }
        return;
      }
      using child_type = typename ChildTraits::storage_type;
      std::map<std::uint64_t, const child_type*> old_children, incoming_children;
      visit_owned_children(base, [&](const auto& entry) { old_children.emplace(entry.persistent_id, &entry); });
      visit_owned_children(source, [&](const auto& entry) { incoming_children.emplace(entry.persistent_id, &entry); });
      visit_owned_children(target, [&](auto& entry) {
        child<ChildTraits>(entry, *old_children.at(entry.persistent_id),
                          *incoming_children.at(entry.persistent_id));
      });
    }
  }
};
} // namespace detail
} // namespace rohit::managed
