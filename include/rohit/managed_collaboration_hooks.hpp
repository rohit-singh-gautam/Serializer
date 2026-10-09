// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/managed_journal.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::managed {

struct collaboration_client_options {
  std::uint64_t sync_interval_ms{5000};
  std::size_t max_transactions{4096};
  std::size_t max_state_bytes{256 * journal_options::mebibyte};
  std::size_t max_flush_operations{256};
};

template <typename Session>
struct collaboration_session_traits;
template <typename Store, typename Session, typename Policy>
class store_collaboration;

namespace detail {
// Optional store-owned integration. Unattached stores retain their existing journal and history path.
class collaboration_attachment {
public:
  virtual ~collaboration_attachment() = default;
  // Prepare one local transaction before durable publication; cleanup never throws.
  virtual void prepare(const std::vector<std::uint8_t>& snapshot, std::string_view label) = 0;
  virtual void publish() noexcept = 0;
  virtual void abort() noexcept = 0;
  // History creates ordinary atomic store transactions with collaboration inverse metadata.
  virtual void undo(bool redo) = 0;
  // Describe local intent independently of remote snapshot history.
  virtual std::string history_label(bool redo) const = 0;
  // Force a flush, or let the caller's monotonic timer drive the configured interval.
  virtual void synchronize() = 0;
  virtual void send_pending() = 0;
  virtual void receive_changes() = 0;
  virtual bool synchronize_if_due(std::uint64_t now_ms) = 0;
  // The checkpoint binds session, model/history, acknowledgements and exact outstanding requests.
  virtual std::vector<std::uint8_t> save() const = 0;
  virtual void load(const std::vector<std::uint8_t>& bytes) = 0;
  virtual void create_journal(const std::filesystem::path&, journal_storage_mode,
                              journal_options) = 0;
  virtual void recover_journal(const std::filesystem::path&, journal_options) = 0;
};
} // namespace detail
} // namespace rohit::managed
