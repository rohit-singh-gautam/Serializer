// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <ledger.hpp>
#include <rohit/managed.hpp>

#include "allocation_metrics.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using store_type = managed::model_store<ledger>;
constexpr std::size_t payload_bytes = 256;
constexpr std::size_t edit_count = 32;
constexpr std::array entity_counts{std::size_t{64}, std::size_t{512}, std::size_t{4096}};

// Construct reproducible owned payloads outside the timed action regions.
ledger make_ledger(std::size_t entities) {
  ledger value;
  value.name = "history-retention";
  for (std::size_t index = 0; index < entities; ++index) {
    value.entries.emplace(static_cast<std::uint64_t>(index),
        ledger_example::entry{std::string(payload_bytes, 'x'), static_cast<std::int64_t>(index)});
  }
  return value;
}

// Time complete checked actions once each; setters remain inside one atomic transaction.
template <typename Callable>
double timed_action(Callable&& callback) {
  const auto start = std::chrono::steady_clock::now();
  std::forward<Callable>(callback)();
  return std::chrono::duration<double, std::micro>(
      std::chrono::steady_clock::now() - start).count();
}

// Compare retained live/deleted history and pruning using independent, equally sized documents.
void measure(std::size_t entities, bool compact, bool deleted, bool pruned, bool pin_before_delete) {
  qualification::memory::start();
  auto initial = make_ledger(entities);
  managed::store_options options;
  options.max_revisions = edit_count + 2;
  options.compact_history = compact;
  std::array<double, edit_count> edit_microseconds{};
  double deletion_microseconds{};
  double undo_microseconds{};
  double redo_microseconds{};
  double save_microseconds{};
  std::size_t encoded_bytes{};
  std::size_t admitted_bytes{};
  auto store = std::make_unique<store_type>(std::move(initial), managed::document_id{17, 23}, options);
  for (std::size_t index = 0; index < edit_count; ++index) {
    edit_microseconds[index] = timed_action([&] {
      auto outcome = store->execute_transaction([&](auto& transaction) {
        transaction.root().entries().edit(2).set_amount_minor_units(
            static_cast<std::int64_t>(index + 1));
      });
      outcome.throw_if_failed();
      if (outcome.status != managed::transaction_status::committed) {
        throw std::runtime_error{"Benchmark action did not commit"};
      }
    });
  }
  const auto before_delete_id = store->read()->entries.at(0).persistent_id;
  const auto before_delete_pin = pin_before_delete ? store->read() : nullptr;
  if (deleted) {
    deletion_microseconds = timed_action([&] {
      auto outcome = store->execute_transaction([](auto& transaction) {
        transaction.root().entries().erase(2);
      });
      outcome.throw_if_failed();
    });
    if (store->read()->entries.contains(0)) {
      throw std::runtime_error{"Benchmark deletion was not published"};
    }
    undo_microseconds = timed_action([&] { store->undo(); });
    if (store->read()->entries.at(0).persistent_id != before_delete_id) {
      throw std::runtime_error{"Benchmark undo changed persistent identity"};
    }
    redo_microseconds = timed_action([&] { store->redo(); });
  }
  if (pruned) {
    store->reset_history();
  }
  admitted_bytes = store->resident_bytes();
  save_microseconds = timed_action([&] { encoded_bytes = store->save().size(); });
  const auto memory = qualification::memory::finish();
  std::sort(edit_microseconds.begin(), edit_microseconds.end());
  // Nearest-rank percentile uses the same fixed count for every workload.
  constexpr std::size_t percentile = 95;
  constexpr std::size_t percent_scale = 100;
  constexpr std::size_t p95_rank =
      (edit_count * percentile + percent_scale - 1) / percent_scale;
  std::cout << entities << ',' << payload_bytes << ',' << edit_count << ',' << compact << ','
            << deleted << ',' << pruned << ',' << pin_before_delete << ','
            << store->read()->entries.size() << ',' << encoded_bytes << ',' << admitted_bytes << ','
            << memory.allocations << ',' << memory.requested_bytes << ',' << memory.live_bytes << ','
            << memory.peak_bytes << ',' << edit_microseconds[edit_count / 2] << ','
            << edit_microseconds[p95_rank - 1] << ',' << deletion_microseconds << ','
            << undo_microseconds << ',' << redo_microseconds << ',' << save_microseconds << '\n';
  if (before_delete_pin && before_delete_pin->entries.size() != entities) {
    throw std::runtime_error{"Benchmark reader pin lost its immutable source"};
  }
}
} // namespace

// Emit a reproducible CSV without imposing timing thresholds on correctness tests.
int main() {
  try {
    std::cout << "# compiler=" << SERIALIZER_BENCHMARK_COMPILER
              << ",configuration=" << SERIALIZER_BENCHMARK_CONFIGURATION << '\n';
    std::cout << "# Allocation counts cover C++ new requests during the workload; preexisting input, "
                 "malloc-backed codec streams, allocator metadata and process RSS are excluded.\n";
    std::cout << "entities,payload_bytes,edits,compact,deleted,pruned,pinned,live_entities,envelope_bytes,"
                 "admitted_bytes,new_calls,new_requested_bytes,new_live_bytes,new_peak_bytes,"
                 "edit_median_us,edit_p95_us,delete_us,undo_us,redo_us,save_us\n";
    for (const auto entities : entity_counts) {
      for (const bool compact : {false, true}) {
        measure(entities, compact, false, false, false);
        measure(entities, compact, true, false, false);
        measure(entities, compact, true, true, false);
        measure(entities, compact, true, true, true);
      }
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
