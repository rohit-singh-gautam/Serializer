#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
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

struct encode_limits {
  std::size_t max_input_bytes{64 * mebibyte};
  std::size_t max_output_bytes{64 * mebibyte};
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
