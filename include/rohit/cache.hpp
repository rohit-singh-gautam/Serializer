// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/digest.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace rohit::cache {

using fingerprint = std::array<std::uint8_t, digest_size(digest_algorithm::sha256)>;
using bytes = std::vector<std::uint8_t>;
inline constexpr std::uint8_t default_priority = 0;

// Generated declaration identity is independent of payload IDs and target naming profiles.
struct descriptor {
  std::string_view identity{};
  std::uint8_t priority{default_priority};
};

// Access an explicitly declared disposable field; managed publication still uses runtime editors.
template <typename Owner, typename Value>
struct member {
  descriptor metadata{};
  Value Owner::* address{};
  // Borrow a field while its owning model remains alive.
  const Value& read(const Owner& owner) const {
    return owner.*address;
  }
  // Replace a field on an application-owned value or isolated candidate.
  void assign(Owner& owner, Value value) const {
    owner.*address = std::move(value);
  }
};
template <typename Owner>
struct fields;

// Every discriminator participates in the disk key; source/recipe changes cannot reuse a hit.
struct key {
  std::uint64_t document_high{};
  std::uint64_t document_low{};
  std::uint64_t entity{};
  fingerprint declaration{};
  fingerprint source{};
  fingerprint recipe{};
  fingerprint parameters{};
  std::uint8_t declared_priority{}; // Policy hint; deliberately excluded from source identity.
  // Compare source identities without making a priority adjustment invalidate otherwise valid data.
  auto operator<=>(const key& other) const {
    return std::tie(document_high, document_low, entity, declaration, source, recipe,
                    parameters) <=> std::tie(other.document_high, other.document_low, other.entity,
                                             other.declaration, other.source, other.recipe,
                                             other.parameters);
  }
  // Policy hints do not participate in key equality or disk filenames.
  bool operator==(const key& other) const {
    return (*this <=> other) == 0;
  }
};

// Bind exact durable source bytes, a stable declaration/override, and an explicit recipe identity.
// Hashes are integrity/identity bindings, not authentication of a derived result.
key make_key(std::uint64_t document_high, std::uint64_t document_low, std::uint64_t entity,
             descriptor declaration, fingerprint source, std::string_view recipe,
             std::span<const std::uint8_t> parameters = {});

// Derive Serializer's shared companion directory; no directory is created by this function.
std::filesystem::path companion_directory(const std::filesystem::path& document);

enum class status { absent, building, ready, failed };
enum class io_event { before_stage, after_stage, before_publish, after_publish, before_remove };

struct entry {
  key identity{};
  std::uint8_t priority{};
  bytes value{};
};
struct info {
  key identity{};
  std::uint8_t priority{};
  std::int64_t last_access{};
  std::uint64_t stored_bytes{};
  bool pinned{};
};
struct reclaim_result {
  std::uint64_t removed_file_bytes{};
  std::uint64_t reclaimed_bytes{}; // Observed increase in available filesystem bytes.
  std::uint64_t shortfall_bytes{};
  std::uint64_t pinned_bytes{};
  std::size_t removed_entries{};
  std::exception_ptr error{};
};

// Optional storage boundary. Implementations coordinate readers/writers and own their paths.
// Returned owning pins remain immutable; storage failures must not mutate authoritative data.
class provider {
public:
  // Release resources without deleting persisted cache entries.
  virtual ~provider() = default;
  // Return a complete exact-key entry, or a miss; malformed optional bytes are never a hit.
  virtual std::shared_ptr<const entry> lookup(const key& identity) = 0;
  // Publish a bounded complete entry atomically; throws on failed admission or I/O.
  virtual void publish(const key& identity, std::span<const std::uint8_t> value,
                       std::optional<std::uint8_t> priority = {}) = 0;
  // Invalidate one known entry; return false if the implementation postpones physical removal.
  virtual bool invalidate(const key& identity) = 0;
  // Return bounded metadata, without pinning payloads.
  virtual std::vector<info> enumerate() = 0;
  // Remove eligible entries by ascending priority, then LRU, then key; report physical shortfall.
  virtual reclaim_result reclaim(std::uint64_t target_bytes) = 0;
};

struct directory_options {
  static constexpr std::size_t mebibyte = 1024 * 1024;
  std::size_t max_entry_bytes{64 * mebibyte};
  std::size_t max_total_bytes{256 * mebibyte};
  std::size_t max_entries{4096};
  std::size_t max_scan_entries{65536};
  std::uint64_t reserve_free_bytes{64 * mebibyte};
  std::function<void(io_event)> fault_injector{};
};

// One exclusive provider owns one explicit cache domain. Share its instance across local threads;
// competing processes fail to acquire cache.lock rather than race publication and cleanup.
class directory_provider final : public provider {
  struct implementation;
  std::unique_ptr<implementation> state_;

public:
  // Create/open only this cache directory, rejecting symlink roots and invalid limits.
  explicit directory_provider(std::filesystem::path directory, directory_options options = {});
  // Release cache.lock, retaining persisted entries.
  ~directory_provider() override;
  // Validate the complete record and update its LRU timestamp before returning an owning pin.
  std::shared_ptr<const entry> lookup(const key& identity) override;
  // Stage, sync and atomically replace an exact-key entry with bounded disk-space admission.
  void publish(const key& identity, std::span<const std::uint8_t> value,
               std::optional<std::uint8_t> priority = {}) override;
  // Remove a known disk entry immediately; existing pins own independent immutable bytes.
  bool invalidate(const key& identity) override;
  // Enumerate validated entries and purge unpinned corrupt records in the owned namespace.
  std::vector<info> enumerate() override;
  // Reclaim by validity/priority/LRU without touching journal, state or unrelated files.
  reclaim_result reclaim(std::uint64_t target_bytes) override;
};

struct result {
  status state{status::absent};
  std::shared_ptr<const entry> pin{};
  std::exception_ptr error{};
  std::exception_ptr storage_error{}; // A ready rebuilt value can survive optional disk failure.
};
struct request_options {
  std::optional<std::uint8_t> priority{}; // Absent means the generated declaration's priority.
  std::size_t max_value_bytes{directory_options{}.max_entry_bytes};
  std::function<void()> check_cancelled{};
  std::function<bool(std::span<const std::uint8_t>)> validate{};
  std::function<bool()> source_is_current{};
};

// Coalesce synchronous rebuilds per exact key. Callbacks run outside service/provider locks.
// Rebuild work, external dependency identity, authorization and background scheduling belong to
// the application; there is no automatic retry loop or callback during document decoding.
class service {
  struct implementation;
  std::unique_ptr<implementation> state_;

public:
  // Supply optional persistence; a null provider creates an in-memory cache service.
  explicit service(std::shared_ptr<provider> storage = {}, std::size_t max_entries = 4096,
                   std::size_t max_resident_bytes = 256 * directory_options::mebibyte);
  // Destroy only after calls have joined; owning result pins remain valid.
  ~service();
  // Read or rebuild once, reporting cancellation/validation/stale-source failures as failed.
  result get_or_rebuild(const key& identity, const std::function<bytes()>& rebuild,
                        request_options options = {});
  // Inspect the current state without rebuilding or exposing a partially written value.
  status state(const key& identity) const;
  // Cancel future publication for this key and invalidate optional storage; existing pins survive.
  void invalidate(const key& identity);
};

} // namespace rohit::cache
