// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml_events.hpp>

#include <functional>
#include <memory>
#include <string_view>

namespace rohit::serializer::xml {

// One synchronous observer; every view expires when this call returns.
using event_callback = std::function<void(const event_view&)>;

// Incremental UTF-8 XML 1.0 producer backed by the optional XML provider, without a tree.
// The callback is owned; its borrowed captures and context must outlive use.
// Calls are neither thread-safe nor reentrant.
// Limits account logical event state; provider buffers/dictionaries are not a heap quota.
class event_reader {
  struct implementation;
  std::unique_ptr<implementation> state_;

public:
  // Start a single parse; an absent callback or unavailable XML provider throws immediately.
  explicit event_reader(event_callback callback, event_limits bounds = {},
                        operation_context* context = nullptr);
  // Release provider state without publishing completion for an unfinished document.
  ~event_reader();
  // A parse has one owner; copying or moving would invalidate provider callback addresses.
  event_reader(const event_reader&) = delete;
  // Replacing a parse would discard its failure and completion state.
  event_reader& operator=(const event_reader&) = delete;
  // Provider callbacks borrow a stable owner throughout the parse.
  event_reader(event_reader&&) = delete;
  // Keep the callback owner at a stable address.
  event_reader& operator=(event_reader&&) = delete;
  // Admit bytes cumulatively; arbitrary UTF-8/token splits are supported, including empty input.
  // Every error permanently fences the reader. Callback exceptions retain their original type.
  void feed(std::string_view bytes);
  // Check byte EOF before delivering end_document; idempotent after successful completion.
  void finish();
  // Completion requires successful finish, including the final observer callback.
  [[nodiscard]] bool complete() const noexcept;
  // Report terminal syntax, resource, interruption, observer or reentrancy failure.
  [[nodiscard]] bool failed() const noexcept;
  // Inspect the bounded validator's cumulative logical event admissions.
  [[nodiscard]] const event_usage& usage() const noexcept;
  // Inspect cumulative admitted source bytes, independently of decoded event payload sizes.
  [[nodiscard]] std::size_t input_bytes() const noexcept;
};

} // namespace rohit::serializer::xml
