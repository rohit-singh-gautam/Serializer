// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <vector>

namespace rohit::serializer::compression {

inline constexpr std::size_t mebibyte = 1024 * 1024;
enum class format { none, zstd, lz4, gzip, zlib, deflate, custom };
enum class error_code { unavailable, invalid_options, invalid_data, resource_limit };

// Compression failures carry a stable category and never include message payloads.
class error : public std::runtime_error {
public:
  const error_code code;
  // Capture the category and a backend-independent diagnostic.
  error(error_code code, const char* message) : std::runtime_error{message}, code{code} {}
};

// Borrow a callable object without allocation; copies retain the same non-owning reference.
// The callable must remain valid at a stable address for every invocation, including through
// a streaming encoder. No synchronization is added; callable exceptions propagate unchanged.
class cancellation_callback {
  const void* context_{};
  void (*invoke_)(const void*){};

  // Invoke the original lvalue while preserving its const or mutable call operator.
  template <typename Callback>
  static void invoke_callback(const void* context) {
    std::invoke(*static_cast<Callback*>(const_cast<void*>(context)));
  }

public:
  // Keep the disabled state literal so existing constexpr limit policies remain valid.
  constexpr cancellation_callback() noexcept = default;
  // Accept an explicit disabled hook without retaining any callable.
  constexpr cancellation_callback(std::nullptr_t) noexcept {}
  // Borrow only lvalue callable objects; temporary callables would leave dangling references.
  template <typename Callback>
    requires(std::is_lvalue_reference_v<Callback&&> &&
             std::is_object_v<std::remove_reference_t<Callback>> &&
             !std::is_volatile_v<std::remove_reference_t<Callback>> &&
             !std::same_as<std::remove_cvref_t<Callback>, cancellation_callback> &&
             std::invocable<Callback&>)
  constexpr cancellation_callback(Callback&& callback) noexcept
      : context_{std::addressof(callback)},
        invoke_{&invoke_callback<std::remove_reference_t<Callback>>} {}
  // Report whether a callable object was supplied.
  constexpr explicit operator bool() const noexcept { return invoke_ != nullptr; }
  // Poll the borrowed callable, or do nothing for an empty hook.
  void operator()() const {
    if (invoke_) { invoke_(context_); }
  }
};

struct encode_limits {
  std::size_t max_input_bytes{64 * mebibyte};
  std::size_t max_output_bytes{64 * mebibyte};
  // Throw to cancel at each built-in provider output boundary; custom providers must cooperate.
  cancellation_callback check_cancelled{};
};

// Implementations emit chunks through this bounded sink; it owns no backend state.
class output_sink {
public:
  // Release a polymorphic sink without finalizing fallible I/O.
  virtual ~output_sink() = default;
  // Append a chunk or throw before exceeding the caller's output limit.
  virtual void append(std::span<const std::uint8_t> bytes) = 0;
};

// Optional application adapters borrow their configuration and must not retain input or sink.
// Decode must validate one complete stream, reject trailing data, and enforce its window bound.
// Implementations own their resource/thread-safety policy; no global registration is required.
class backend {
public:
  // Release an adapter; calls never transfer ownership to Serializer.
  virtual ~backend() = default;
  // Emit one finalized standard stream; configuration belongs to the adapter instance.
  virtual void encode(std::span<const std::uint8_t> input, output_sink& output) const = 0;
  // Validate one stream and emit bytes under the supplied history-window limit.
  virtual void decode(std::span<const std::uint8_t> input, output_sink& output,
                      std::size_t max_window_bytes) const = 0;
};

struct none_options {};
struct zstd_options {
  int level{3};
  bool checksum{true};
};
struct lz4_options {
  int level{0};
  bool checksum{true};
};
struct gzip_options {
  int level{6};
};
struct zlib_options {
  int level{6};
};
struct deflate_options {
  int level{6};
};
struct custom_options {
  const backend* implementation{};
};
using encode_options = std::variant<none_options, zstd_options, lz4_options, gzip_options,
                                    zlib_options, deflate_options, custom_options>;

struct decode_options {
  compression::format format{format::none};
  std::size_t max_compressed_bytes{64 * mebibyte};
  std::size_t max_decompressed_bytes{64 * mebibyte};
  std::size_t max_window_bytes{8 * mebibyte};
  const backend* custom_backend{};
  // Throw to cancel at each built-in provider output boundary, including empty output batches.
  cancellation_callback check_cancelled{};
};

// Report compiled-in built-in formats; custom adapters are selected explicitly per call.
[[nodiscard]] bool available(format selected) noexcept;
// Validate options and availability before callers begin serialization or consume input.
void validate(const encode_options& options);
// Validate the selected backend and resource policy without consuming input.
void validate(const decode_options& options);
// Encode a bounded span to a standard frame/member; failure returns no partial result.
[[nodiscard]] std::vector<std::uint8_t> compress(std::span<const std::uint8_t> input,
                                                 const encode_options& options,
                                                 encode_limits limits = {});
// Decode exactly one frame/member, including trailer checks; concatenation is rejected.
// Window limits are separate from output storage and are not a total process-memory budget.
[[nodiscard]] std::vector<std::uint8_t> decompress(std::span<const std::uint8_t> input,
                                                   const decode_options& options);

} // namespace rohit::serializer::compression
