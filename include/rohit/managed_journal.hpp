// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/digest.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace rohit::managed {

enum class journal_storage_mode : std::uint64_t { appended = 1, sidecar = 2 };

// A failure after issuing writes may have committed. Reopen before attempting another operation.
class journal_indeterminate_error : public std::runtime_error {
public:
  // Describe the uncertain storage operation; the original exception is nested when available.
  explicit journal_indeterminate_error(const char* message) : std::runtime_error{message} {}
};

// Deterministic interruption boundaries for storage qualification, not application notifications.
enum class journal_io_event {
  before_append,
  after_header,
  after_payload,
  after_commit,
  after_flush,
  after_sidecar_flush,
  after_base_flush,
  before_replace,
  after_replace,
  after_publish,
  before_cleanup,
  before_tail_repair,
  after_tail_repair,
  before_prepare_publish,
  after_prepare_publish,
  before_confirm,
  after_confirm_decision
};

// Host-supplied evidence for independently published bytes; authenticity belongs to the host.
struct journal_publication_evidence {
  std::uint64_t document_bytes{};
  std::array<std::uint8_t, digest_size(digest_algorithm::sha256)> document_digest{};
};

// A durable, unresolved Save. Publish document bytes externally, then confirm the opaque token.
// Owning buffers are bounded by journal options; no application effects run inside Serializer.
struct journal_prepared_save {
  std::vector<std::uint8_t> token{};
  std::vector<std::uint8_t> document{};
  std::vector<std::uint8_t> origin{};
};

// Adapt an application document format to canonical managed root bytes. Callbacks are synchronous,
// must preserve all durable model values, and must bound their own format-specific decode work.
struct journal_document_codec {
  std::string format{};
  std::function<std::vector<std::uint8_t>(std::span<const std::uint8_t>)> encode{};
  std::function<std::vector<std::uint8_t>(std::span<const std::uint8_t>)> decode{};
};

struct journal_options {
  static constexpr std::size_t mebibyte = 1024 * 1024;
  std::size_t max_record_bytes{64 * mebibyte};
  std::size_t max_file_bytes{1024 * mebibyte};
  // Tests may throw or terminate here. Never reenter the store or perform application effects.
  std::function<void(journal_io_event)> fault_injector{};
  // Sidecar-only document adapter; omit to retain the native managed baseline container.
  std::shared_ptr<const journal_document_codec> document_codec{};
  // Bound cumulative reconstructed bytes in a differential generation, independently of disk size.
  std::size_t max_replay_bytes{1024 * mebibyte};
  // Opaque application provenance, used only on creation with a document codec.
  std::vector<std::uint8_t> origin{};
  std::size_t max_origin_bytes{64 * 1024};
};

namespace detail {

// Version-two operation tags; selection and allocation records never carry model snapshots.
enum class journal_record_kind : std::uint8_t {
  edit = 1,
  select = 2,
  reserve = 3,
  reset = 4,
  restore_edit = 5,
  delta_edit = 6,
  delta_restore_edit = 7,
  leased_edit = 8,
  reserve_range = 9,
  merge_checkpoint = 10,
  merge_delta = 11
};

// Write a little-endian word into a caller-owned fixed prefix without allocation.
void journal_put_word(std::span<std::uint8_t> bytes, std::size_t offset, std::uint64_t value);
// Read one wire integer with bounds checking and advance the byte cursor.
std::uint64_t journal_get_word(std::span<const std::uint8_t> bytes, std::size_t& offset);

// Storage-independent managed commit boundary. Byte encoding uses stream concepts underneath.
// Implementations own their resources and must finish all fallible work before acknowledging writes.
// A sink may report an indeterminate result; callers must fence live publication until recovery.
class journal_sink {
public:
  // Release backend resources without implicitly saving or changing the durable decision.
  virtual ~journal_sink() = default;
  // Repair only a validated incomplete tail and establish a durable recovery position.
  virtual void finish_recovery() = 0;
  // Persist borrowed metadata/label/snapshot before returning; never retain the borrowed buffers.
  virtual void append(std::span<const std::uint8_t> prefix,
                      std::span<const std::uint8_t> label = {},
                      std::span<const std::uint8_t> snapshot = {}) = 0;
  // Durably replace the saved baseline, preserving its retained history dependencies.
  virtual void save(const std::vector<std::uint8_t>& envelope) = 0;
  // Publish a data-only document with its managed metadata; ordinary sinks retain their Save path.
  virtual void save_document(const std::vector<std::uint8_t>& envelope,
                             std::span<const std::uint8_t>) {
    save(envelope);
  }
  // Report whether Save must supply a normalized, ID-free root for an application document codec.
  virtual bool uses_document_codec() const noexcept { return false; }
  // Read the selected base's opaque provenance, independently of authored content.
  virtual std::vector<std::uint8_t> origin() const {
    throw std::logic_error{"Journal backend does not support origin metadata"};
  }
  // Replace provenance with a full native Save using the ordinary publication protocol.
  virtual void save_document_origin(const std::vector<std::uint8_t>&,
                                    std::span<const std::uint8_t>, std::span<const std::uint8_t>) {
    throw std::logic_error{"Journal backend does not support origin replacement"};
  }
  // Pin one exact native checkpoint and proposed base; further durable mutations must reject.
  virtual journal_prepared_save prepare_document_save(const std::vector<std::uint8_t>&,
      std::span<const std::uint8_t>, std::span<const std::uint8_t>) {
    throw std::logic_error{"Journal backend does not support prepared Save"};
  }
  // Expose an unresolved durable preparation after restart without guessing external success.
  virtual std::optional<journal_prepared_save> prepared_save() const { return {}; }
  // Query the mutation barrier without allocating or reading staged document bytes.
  virtual bool has_prepared_save() const noexcept { return false; }
  // Return true only for a newly activated base; exact retries return false without mutation.
  virtual bool confirm_document_save(std::span<const std::uint8_t>,
                                     const journal_publication_evidence&) {
    throw std::logic_error{"Journal backend does not support confirmed Save"};
  }
  // Read the last full-Save envelope lazily; custom sinks may explicitly reject this capability.
  virtual std::vector<std::uint8_t> saved_envelope() const {
    throw std::logic_error{"Journal backend does not support saved baseline reads"};
  }
  // Replace the saved generation as one new durable operation, preserving ordinary Save semantics.
  virtual void discard(const std::vector<std::uint8_t>&) {
    throw std::logic_error{"Journal backend does not support discard"};
  }
  // Return the last acknowledged operation sequence, independently of undo position.
  virtual std::uint64_t sequence() const noexcept = 0;
  // Report an uncertain durable decision that blocks further writes.
  virtual bool needs_recovery() const noexcept = 0;
};

} // namespace detail
} // namespace rohit::managed
