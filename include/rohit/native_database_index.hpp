// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/native_database.hpp>
#include <rohit/paged_index.hpp>

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::managed {

// Bind an explicit local disk accelerator to a pinned eager database source. This does not change
// data authority, provision database placement, or make owning payload/history materialization lazy.
template <typename Root, typename Descriptor, typename Traits = model_traits<Root>>
  requires std::same_as<typename Descriptor::value_type, std::uint64_t> &&
    requires(const std::remove_cvref_t<Descriptor>& selected) {
      { selected.index() } -> std::same_as<index_info>;
    }
class native_database_index_snapshot {
public:
  using storage_type = typename Traits::storage_type;
  using plain_type = typename Descriptor::plain_type;
  using entity_storage_type = typename Descriptor::storage_type;
  using descriptor_type = std::remove_cvref_t<Descriptor>;
private:
  std::shared_ptr<const storage_type> source_;
  document_id document_;
  descriptor_type descriptor_;
  paged_index_binding binding_;
  std::unique_ptr<paged_index> index_;

  // Hash stable field/type identity and caller-selected extraction generation, never ABI names.
  static std::array<std::uint8_t, digest_size(digest_algorithm::sha256)> definition_digest(
      const Descriptor& descriptor, std::string_view definition) {
    if (definition.empty() || descriptor.info.id == 0) {
      throw std::invalid_argument{"Database index requires a stable field and definition identity"};
    }
    const auto definition_identity = make_digest<digest_algorithm::sha256>(definition);
    std::vector<std::uint8_t> bytes(definition_identity.begin(), definition_identity.end());
    detail::database_put_word(bytes, model_traits<plain_type>::class_id);
    detail::database_put_word(bytes, descriptor.info.id);
    constexpr std::uint64_t comparator_version = 1;
    detail::database_put_word(bytes, comparator_version);
    return make_digest<digest_algorithm::sha256>(bytes);
  }

  // Resolve generated ownership under an explicit work cap, without a second object directory.
  static entity_read<plain_type> resolve(const std::shared_ptr<const storage_type>& source,
      document_id document, std::uint64_t identity, std::size_t max_entities) {
    if (!source) { throw std::logic_error{"Moved database index source snapshot"}; }
    const entity_storage_type* selected{};
    std::size_t visited{};
    detail::visit_registry_entities<Traits>(*source, [&]<typename Actual>(const auto& value) {
      if (visited == max_entities) { throw std::length_error{"Database indexed load work budget exceeded"}; }
      ++visited;
      if constexpr (std::same_as<Actual, plain_type>) {
        if (value.persistent_id == identity) { selected = &value; }
      }
    });
    if (!selected) {
      throw registry_error{registry_error_code::missing_id, "Indexed entity is absent from its source snapshot"};
    }
    return entity_read<plain_type>{std::shared_ptr<const entity_storage_type>{source, selected}, document};
  }

public:
  // Couple every location stream to its exact eager source, even after its snapshot owner is closed.
  class cursor {
    std::shared_ptr<const storage_type> source_;
    paged_index_binding binding_;
    paged_index::cursor locations_;
    friend class native_database_index_snapshot;
    // Retain both source and index generations without allocating another owning payload.
    cursor(std::shared_ptr<const storage_type> source, paged_index_binding binding,
           paged_index::cursor locations)
        : source_{std::move(source)}, binding_{std::move(binding)}, locations_{std::move(locations)} {}
  public:
    cursor(const cursor&) = delete;
    cursor& operator=(const cursor&) = delete;
    // Transfer the paired source/index generations and exact traversal position.
    cursor(cursor&&) noexcept = default;
    // Release old paired pins and retain the selected source/index cursor.
    cursor& operator=(cursor&&) noexcept = default;
    // Copy one key/identity pair while retaining the original source and leaf page pins.
    std::optional<paged_index_entry> next() { return locations_.next(); }
    // Resolve an identity explicitly against this cursor's own source, never the latest database.
    entity_read<plain_type> load(std::uint64_t identity, std::size_t max_entities) const {
      return resolve(source_, {binding_.document_high, binding_.document_low}, identity, max_entities);
    }
    // Inspect the exact source/revision coordinates accompanying every copied location.
    const paged_index_binding& binding() const noexcept { return binding_; }
    // Retain source ownership independently of the cursor's current page or traversal lifetime.
    std::shared_ptr<const storage_type> source() const noexcept { return source_; }
  };

  // Capture source/model identity synchronously; a later durable commit cannot change this pin.
  native_database_index_snapshot(const native_database<Root, Traits>& database,
      Descriptor descriptor, std::string_view definition,
      std::size_t max_source_bytes = store_options::default_snapshot_bytes)
      : source_{database.read()}, document_{database.document()}, descriptor_{descriptor} {
    static_assert(std::same_as<entity_storage_type, typename model_traits<plain_type>::storage_type>);
    binding_.document_high = document_.high;
    binding_.document_low = document_.low;
    binding_.authority_epoch = database.epoch();
    binding_.source_revision = database.revision();
    binding_.schema_digest = make_digest<digest_algorithm::sha256>(std::string_view{Traits::schema_id});
    binding_.definition_digest = definition_digest(descriptor_, definition);
    binding_.source_digest = make_digest<digest_algorithm::sha256>(detail::encode(*source_, max_source_bytes));
  }

  native_database_index_snapshot(const native_database_index_snapshot&) = delete;
  native_database_index_snapshot& operator=(const native_database_index_snapshot&) = delete;
  // Move source and derived generation together without reopening mutable paths.
  native_database_index_snapshot(native_database_index_snapshot&&) noexcept = default;
  // Replace both pins together; already returned immutable entity handles remain independently valid.
  native_database_index_snapshot& operator=(native_database_index_snapshot&&) noexcept = default;

  // Stream genuine generated entity identities and selected uint64 values through external sorting.
  // This rebuild is derived work: failure never changes an accepted database outcome or source pin.
  paged_index_build_statistics rebuild(const std::filesystem::path& path,
                                      const paged_index_options& options = {}) {
    return paged_index::build_from(path, binding_, [&](const detail::paged_index_emit& emit) {
      detail::visit_registry_entities<Traits>(*source_, [&]<typename Actual>(const auto& value) {
        if constexpr (std::same_as<Actual, plain_type>) {
          emit({descriptor_.read(value), value.persistent_id});
        }
      });
    }, options);
  }

  // Open a matching complete index; stale/ahead/foreign generations reject before any query result.
  void open(const std::filesystem::path& path, const paged_index_options& options = {}) {
    auto prepared = std::make_unique<paged_index>(path, binding_, options);
    index_.swap(prepared);
  }

  // Return identity locations with their exact source pin, independently of this wrapper's lifetime.
  cursor equal(std::uint64_t key) const {
    if (!index_) { throw std::logic_error{"Database snapshot index is not ready"}; }
    return cursor{source_, binding_, index_->equal(key)};
  }

  // Query a numeric range without materializing a result vector or an O(N) in-memory directory.
  cursor range(std::uint64_t lower, std::uint64_t upper) const
    requires(descriptor_type::method == index_method::ordered) {
    if (!index_) { throw std::logic_error{"Database snapshot index is not ready"}; }
    return cursor{source_, binding_, index_->range(lower, upper)};
  }

  // Explicitly resolve one document/type-scoped identity against this eager source under a work cap.
  // Resolution scans native ownership without allocating a second object directory; it performs no I/O.
  entity_read<plain_type> load(std::uint64_t identity, std::size_t max_entities) const {
    return resolve(source_, document_, identity, max_entities);
  }

  // Retain an explicit immutable source independently of current database state and index paths.
  std::shared_ptr<const storage_type> source() const noexcept { return source_; }
  // Inspect every durable source/definition coordinate used to reject stale accelerators.
  const paged_index_binding& binding() const noexcept { return binding_; }
  // Inspect the derived cache without claiming a bound on this eager owning source model.
  paged_index_statistics statistics() const {
    if (!index_) { throw std::logic_error{"Database snapshot index is not ready"}; }
    return index_->statistics();
  }
};

} // namespace rohit::managed
