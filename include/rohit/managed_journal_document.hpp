// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/decode.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_journal.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::managed {

// Adapt owning Root codecs to normalized native storage without exposing managed identities.
// Callbacks are copied into the adapter and run synchronously; borrowed dependencies must outlive it.
// Encode accepts const Root& and returns owning bytes; Decode accepts a borrowed byte span and
// returns Root. Callbacks enforce their external-format bounds; limits bound native conversion.
// Callback and conversion failures propagate. Callbacks must not reenter the attached store.
template <typename Root, typename Traits = model_traits<Root>, typename Encode, typename Decode>
[[nodiscard]] std::shared_ptr<const journal_document_codec>
make_journal_document_codec(std::string format, Encode encode, Decode decode,
                            serializer::decode_limits limits = {}) {
  using bytes = std::vector<std::uint8_t>;
  using storage_type = typename Traits::storage_type;
  static_assert(std::copy_constructible<Encode> && std::copy_constructible<Decode>,
                "Journal document callbacks must be copy constructible");
  static_assert(std::same_as<std::invoke_result_t<Encode&, const Root&>, bytes>,
                "Journal document encoder must return std::vector<std::uint8_t>");
  static_assert(std::same_as<std::invoke_result_t<Decode&, std::span<const std::uint8_t>>, Root>,
                "Journal document decoder must return the owning root value");
  if (format.empty()) {
    throw std::invalid_argument{"Journal document format is empty"};
  }

  auto result = std::make_shared<journal_document_codec>();
  result->format = std::move(format);
  // Normalize before the value callback so direct-storage models do not expose recovery IDs.
  result->encode = [encode = std::move(encode),
                    limits](std::span<const std::uint8_t> input) mutable {
    auto storage = detail::decode<storage_type>(input, limits);
    Traits::visit_entities(storage, [](auto& identity, auto) { identity = 0; });
    detail::reset_runtime_fields<Traits>(storage);
    const auto value = Traits::clone_value(storage);
    return std::invoke(encode, value);
  };
  // External IDs are never authoritative; the journal restores its own identity projection.
  result->decode = [decode = std::move(decode),
                    limits](std::span<const std::uint8_t> input) mutable {
    auto storage = Traits::make_storage(std::invoke(decode, input));
    Traits::visit_entities(storage, [](auto& identity, auto) { identity = 0; });
    detail::reset_runtime_fields<Traits>(storage);
    return detail::encode(storage, limits.max_input_bytes);
  };
  return result;
}

} // namespace rohit::managed
