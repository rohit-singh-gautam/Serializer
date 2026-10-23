// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include <rohit/managed_file_journal.hpp>

#include <rohit/file_stream.hpp>
#include <rohit/digest.hpp>
#include <rohit/managed_journal.hpp>
#include <rohit/managed_journal_stream.hpp>
#include "managed_journal_delta.hpp"
#include "managed_journal_document.hpp"
#include "native_file_path.hpp"

#include <array>
#include <algorithm>
#include <exception>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace rohit::managed::detail {
namespace {
using bytes = std::vector<std::uint8_t>;
constexpr std::size_t file_header_bytes = 9 * wire_word_bytes;
constexpr std::uint64_t file_magic = 0x314c4e4a5a5253;
constexpr std::uint64_t record_version = 2;
constexpr std::uint64_t differential_record_version = 3;
constexpr std::uint64_t base_file_kind = 1;
constexpr std::uint64_t sidecar_file_kind = 2;
constexpr std::string_view journal_directory_suffix{".journal"};
constexpr std::string_view journal_lock_name{"writer.lock"};
constexpr std::string_view journal_manifest_name{"manifest"};
constexpr std::string_view journal_pending_manifest_name{"manifest.pending"};
constexpr std::string_view journal_prepared_name{"save.prepared"};
constexpr std::size_t save_token_bytes = journal_save_token_bytes;
constexpr std::uint64_t save_token_magic = 0x315641534a5a5253;
constexpr std::size_t token_header_offset = 2 * wire_word_bytes;
constexpr std::size_t token_sequence_offset = token_header_offset + file_header_bytes;
constexpr std::size_t token_record_digest_offset = token_sequence_offset + wire_word_bytes;
constexpr std::size_t token_manifest_digest_offset = token_record_digest_offset + wire_word_bytes;
constexpr std::size_t token_document_size_offset = token_manifest_digest_offset +
    digest_size(digest_algorithm::sha256);
constexpr std::size_t token_document_digest_offset = token_document_size_offset + wire_word_bytes;
static_assert(token_document_digest_offset + digest_size(digest_algorithm::sha256) + wire_word_bytes ==
              save_token_bytes);

// Reject arbitrary host tokens before inspecting fixed fields; tokens are integrity bindings, not credentials.
void validate_save_token(std::span<const std::uint8_t> token) {
  if (token.size() != save_token_bytes || get_word(token, 0) != save_token_magic ||
      get_word(token, 1) != 1 || checksum(token.first(save_token_bytes - wire_word_bytes)) !=
          get_word(token.last(wire_word_bytes), 0)) {
    throw std::invalid_argument{"Invalid prepared journal Save token"};
  }
}

// Compare exact borrowed byte bindings without allocating on the confirmation path.
bool same_bytes(std::span<const std::uint8_t> left, std::span<const std::uint8_t> right) {
  return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

// Keep auxiliary names beside the canonical document without narrowing Unicode filenames.
std::filesystem::path journal_directory(const std::filesystem::path& path) {
  std::filesystem::path name{"."};
  name += path.filename().native();
  name += journal_directory_suffix;
  return path.parent_path() / name;
}

// Inspect physical companion state through the transient native boundary, not a new identity.
bool native_exists(const std::filesystem::path& path) {
  const rohit::detail::native_file_path native{path};
  return std::filesystem::exists(native.get());
}
// Remove only the existing journal-selected name with the same throwing error contract.
bool native_remove(const std::filesystem::path& path) {
  const rohit::detail::native_file_path native{path};
  return std::filesystem::remove(native.get());
}
// Keep best-effort cleanup's existing error-code behavior even if path normalization fails.
bool native_remove(const std::filesystem::path& path, std::error_code& error) {
  try {
    const rohit::detail::native_file_path native{path};
    return std::filesystem::remove(native.get(), error);
  } catch (const std::system_error& failure) {
    error = failure.code();
    return false;
  }
}

// Make the new dot-prefixed directory hidden in Explorer as well as Unix directory listings.
void prepare_journal_directory(const std::filesystem::path& path) {
  const rohit::detail::native_file_path native{path};
  std::filesystem::create_directory(native.get());
  if (std::filesystem::symlink_status(native.get()).type() != std::filesystem::file_type::directory) {
    throw std::invalid_argument{"Managed journal auxiliary path must be an ordinary directory"};
  }
#ifdef _WIN32
  const auto attributes = GetFileAttributesW(native.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES ||
      (!(attributes & FILE_ATTRIBUTE_HIDDEN) &&
       !SetFileAttributesW(native.c_str(), attributes | FILE_ATTRIBUTE_HIDDEN))) {
    throw std::system_error{static_cast<int>(GetLastError()), std::system_category(),
        "Hide managed journal directory"};
  }
#endif
  // The parent entry must be durable before a document can select files inside it.
  sync_parent_directory(path);
}

// A cross-directory same-filesystem publication changes both parents, including staging removal.
void sync_publication(const std::filesystem::path& source, const std::filesystem::path& target) {
  sync_parent_directory(target);
  if (source.parent_path() != target.parent_path()) { sync_parent_directory(source); }
}

// Preserve nested causes without a forwarding constructor hijacking Windows Clang EH copies.
class nested_journal_error final : public journal_indeterminate_error,
                                   public std::nested_exception {
public:
  // Capture the active exception while keeping the public journal error type and message.
  explicit nested_journal_error(const char* message)
      : journal_indeterminate_error{message}, std::nested_exception{} {}
  // Copy the retained cause rather than recursively capturing the exception being copied.
  nested_journal_error(const nested_journal_error&) = default;
};

// Bound the physical input before allocating and reading the file into memory.
bytes read_file(const std::filesystem::path& path, const journal_options& options,
                std::size_t max_bytes = std::numeric_limits<std::size_t>::max()) {
  file_stream input{path, file_open_mode::read};
  const auto size = input.size();
  if (size > std::min(options.max_file_bytes, max_bytes)) {
    throw std::length_error{"Managed journal file budget exceeded"};
  }
  bytes result(static_cast<std::size_t>(size));
  read_stream_exact(input, result);
  return result;
}

// Cap metadata input before allocation, independently of the potentially much larger document budget.
journal_document_manifest read_manifest(const std::filesystem::path& path,
                                         const journal_options& options) {
  constexpr std::size_t fixed_metadata_bytes = journal_manifest_fixed_bytes + max_codec_id_bytes + 2 * wire_word_bytes +
                                              save_token_bytes;
  const auto budget = options.max_file_bytes <= fixed_metadata_bytes ||
      options.max_origin_bytes > options.max_file_bytes - fixed_metadata_bytes
      ? options.max_file_bytes : fixed_metadata_bytes + options.max_origin_bytes;
  return decode_document_manifest(read_file(path, options, budget), options.max_file_bytes,
                                   options.max_origin_bytes);
}

struct file_header {
  journal_storage_mode mode{};
  std::uint64_t kind{};
  std::uint64_t generation_high{};
  std::uint64_t generation_low{};
  std::uint64_t sequence{};
  std::uint64_t base_digest{};
  std::uint64_t version{record_version};
};

// Encode a versioned header binding physical mode, generation, sequence, and base frame checksum.
bytes encode_header(const file_header& header) {
  bytes result;
  result.reserve(file_header_bytes);
  for (const auto word :
       {file_magic, header.version, static_cast<std::uint64_t>(header.mode), header.kind,
        header.generation_high, header.generation_low, header.sequence, header.base_digest}) {
    put_word(result, word);
  }
  put_word(result, checksum(result));
  return result;
}

// Reject damaged or unsupported headers before trusting their mode or generation.
file_header decode_header(std::span<const std::uint8_t> input, std::uint64_t kind) {
  if (input.size() < file_header_bytes) {
    throw std::invalid_argument{"Incomplete managed journal header"};
  }
  const auto header = input.first(file_header_bytes);
  if (get_word(header, 0) != file_magic ||
      (get_word(header, 1) != record_version && get_word(header, 1) != differential_record_version) ||
      get_word(header, 3) != kind ||
      checksum(header.first(file_header_bytes - wire_word_bytes)) != get_word(header, 8)) {
    throw std::invalid_argument{"Invalid managed journal header"};
  }
  const auto mode = static_cast<journal_storage_mode>(get_word(header, 2));
  if ((mode != journal_storage_mode::appended && mode != journal_storage_mode::sidecar) ||
      (mode == journal_storage_mode::appended && get_word(header, 1) != record_version) ||
      (get_word(header, 4) == 0 && get_word(header, 5) == 0)) {
    throw std::invalid_argument{"Invalid managed journal mode or generation"};
  }
  return {mode,
          kind,
          get_word(header, 4),
          get_word(header, 5),
          get_word(header, 6),
          get_word(header, 7),
          get_word(header, 1)};
}

// Derive companion/staging names from a random generation; filenames alone never establish binding.
std::filesystem::path generation_path(const std::filesystem::path& base, const file_header& header,
                                      const char* role) {
  auto result = base;
  result += std::string{role} + std::to_string(header.generation_high) + "-" +
            std::to_string(header.generation_low);
  return result;
}

// Consume a verified record through the same buffer concept used by ordinary deserialization.
std::optional<decoded_frame> read_file_frame(std::span<const std::uint8_t> bytes,
                                             std::size_t offset, std::uint64_t sequence,
                                             std::uint64_t previous,
                                             const journal_options& options) {
  if (offset > bytes.size()) {
    throw std::invalid_argument{"Invalid managed journal offset"};
  }
  const auto input = make_constant_full_stream(bytes.data() + offset, bytes.size() - offset);
  const auto frame = read_journal_frame(input, sequence, previous, options);
  if (!frame) {
    return {};
  }
  return decoded_frame{frame->payload(), frame->digest,
                       offset + frame_overhead_bytes + frame->payload().size()};
}

// This adapter coordinates file generations only; transport and framing are reusable stream code.
class file_journal final : public journal_sink {
  struct implementation;
  std::unique_ptr<implementation> state_;
  // Acquire the document's stable lock before reading or changing its files.
  file_journal(const std::filesystem::path& path, journal_options options);

public:
  // Release streams and the writer lock without implicitly saving.
  ~file_journal() override;
  // Build a new file-backed baseline without overwriting existing data.
  static std::unique_ptr<file_journal> create(const std::filesystem::path& path,
                                              journal_storage_mode mode, const bytes& envelope,
                                              journal_options options, bool& indeterminate,
                                              std::span<const std::uint8_t> document);
  // Validate container identity and replay its ordered records before tail repair.
  static std::unique_ptr<file_journal>
  recover(const std::filesystem::path& path, journal_options options,
          const std::function<void(bool, std::span<const std::uint8_t>)>& consume);
  // Apply validated tail repair and synchronize accepted data.
  void finish_recovery() override;
  // Commit one borrowed record through a durable file stream.
  void append(std::span<const std::uint8_t> prefix, std::span<const std::uint8_t> label,
              std::span<const std::uint8_t> snapshot) override;
  // Atomically replace the base and rotate the selected companion.
  void save(const bytes& envelope) override;
  // Publish arbitrary document bytes together with their differential managed checkpoint.
  void save_document(const bytes& envelope, std::span<const std::uint8_t> document) override;
  // Let the store supply the normalized model only when an external document adapter is active.
  bool uses_document_codec() const noexcept override;
  // Read checked provenance from the selected native manifest.
  bytes origin() const override;
  // Publish replacement provenance through the existing Save decision.
  void save_document_origin(const bytes&, std::span<const std::uint8_t>,
                            std::span<const std::uint8_t>) override;
  // Persist an unresolved proposed document and complete differential checkpoint.
  journal_prepared_save prepare_document_save(const bytes&, std::span<const std::uint8_t>,
                                              std::span<const std::uint8_t>) override;
  // Return validated staged bytes without resolving external publication.
  std::optional<journal_prepared_save> prepared_save() const override;
  // Keep the unresolved-preparation mutation barrier allocation-free.
  bool has_prepared_save() const noexcept override;
  // Validate host evidence before activating the pinned generation, with exact retry support.
  bool confirm_document_save(std::span<const std::uint8_t>,
                             const journal_publication_evidence&) override;
  // Read only the selected full-Save baseline while retaining the existing writer lock.
  bytes saved_envelope() const override;
  // Publish a restored envelope with a fresh operation sequence.
  void discard(const bytes& envelope) override;
  // Remove saved external-document companions while this backend still owns their lock.
  bool retire_document() override;
  // Return the last acknowledged operation sequence.
  std::uint64_t sequence() const noexcept override;
  // Expose fencing after an uncertain durable operation.
  bool needs_recovery() const noexcept override;
};

struct file_journal::implementation {
  std::filesystem::path path{};
  std::filesystem::path auxiliary_directory{};
  journal_options options{};
  std::unique_ptr<file_stream> lock{};
  std::unique_ptr<file_stream> writer{};
  file_header header{};
  std::filesystem::path append_path{};
  std::uint64_t sequence{};
  std::uint64_t digest{};
  std::size_t append_offset{};
  std::size_t replay_bytes{};
  bool fenced{};
  bool recovering{};
  bool incomplete_tail{};
  std::unique_ptr<journal_delta_base> delta{};
  journal_document_manifest manifest{};
  bool pending_manifest{};
  std::optional<journal_document_manifest> preparation{};

  // Keep unresolved publication state under Serializer's stable writer lock.
  std::filesystem::path prepared_path() const {
    if (!auxiliary_directory.empty()) { return auxiliary_directory / journal_prepared_name; }
    auto result = path;
    result += ".journal.prepared";
    return result;
  }

  // Use a stable side-channel manifest without adding managed data to application document bytes.
  std::filesystem::path manifest_path(bool pending = false) const {
    if (!auxiliary_directory.empty()) {
      return auxiliary_directory / (pending ? journal_pending_manifest_name : journal_manifest_name);
    }
    auto result = path;
    result += pending ? ".journal.pending" : ".journal";
    return result;
  }

  // The saved generation chooses exactly one companion; directory contents are never guessed.
  std::filesystem::path companion_path(const file_header& selected) const {
    return auxiliary_directory.empty() ? generation_path(path, selected, ".journal-")
        : generation_path(auxiliary_directory / "changes", selected, "-");
  }

  // Temporary documents and manifests stay inside the same auxiliary directory as their journal.
  std::filesystem::path staging_path(const file_header& selected) const {
    return auxiliary_directory.empty() ? generation_path(path, selected, ".pending-")
        : generation_path(auxiliary_directory / "pending", selected, "-");
  }

  // Invoke qualification hooks only at documented I/O boundaries.
  void event(journal_io_event point) const {
    if (options.fault_injector) {
      options.fault_injector(point);
    }
  }

  // Enforce fencing even if the caller catches an indeterminate error and attempts another write.
  void require_ready() const {
    if (fenced || recovering) {
      throw journal_indeterminate_error{"Managed journal must be recovered before writing"};
    }
    if (preparation) {
      throw std::logic_error{"Prepared journal Save requires host reconciliation and confirmation"};
    }
  }

  // Validate additive file sizes before arithmetic, allocation, or any publication attempt.
  void check_size(std::size_t prefix, std::size_t payload) const {
    if (prefix > options.max_file_bytes || frame_overhead_bytes > options.max_file_bytes - prefix ||
        payload > options.max_file_bytes - prefix - frame_overhead_bytes) {
      throw std::length_error{"Managed journal file budget exceeded; perform a full Save"};
    }
  }

  // Bound expansion over the complete generation so small patches cannot imply unlimited replay.
  void check_replay(std::size_t prefix, std::size_t payload) const {
    if (prefix > options.max_replay_bytes || payload > options.max_replay_bytes - prefix) {
      throw std::length_error{"Managed journal replay budget exceeded; perform a full Save"};
    }
  }

  // Install a fully flushed generation; until replacement, every failure leaves the old base valid.
  void replace(const bytes& envelope, journal_storage_mode mode, bool existing,
               bool advance_sequence = false, std::span<const std::uint8_t> document = {}) {
    require_ready();
    if (mode != journal_storage_mode::appended && mode != journal_storage_mode::sidecar) {
      throw std::invalid_argument{"Unsupported managed journal storage mode"};
    }
    if (options.document_codec) {
      replace_document(envelope, mode, existing, advance_sequence, document,
                       existing ? std::span<const std::uint8_t>{manifest.origin}
                                : std::span<const std::uint8_t>{options.origin});
      return;
    }
    if (mode == journal_storage_mode::sidecar) {
      check_replay(0, envelope.size());
    }
    check_size(file_header_bytes, envelope.size());
    if (advance_sequence && sequence == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error{"Managed journal sequence exhausted"};
    }
    const auto replacement_sequence = sequence + static_cast<std::uint64_t>(advance_sequence);
    auto frame = encode_frame({envelope}, replacement_sequence, 0, options);
    auto next_delta = mode == journal_storage_mode::sidecar
                          ? std::make_unique<journal_delta_base>(bytes{envelope}) : nullptr;
    std::random_device entropy;
    std::uniform_int_distribution<std::uint64_t> word;
    file_header replacement{mode,          base_file_kind, word(entropy),
                            word(entropy), replacement_sequence, frame.digest,
                            mode == journal_storage_mode::sidecar ? differential_record_version
                                                                 : record_version};
    if (replacement.generation_high == 0 && replacement.generation_low == 0) {
      replacement.generation_low = 1;
    }
    const auto staging = staging_path(replacement);
    auto destination = mode == journal_storage_mode::appended
                           ? path
                           : companion_path(replacement);
    const auto old_sidecar =
        header.mode == journal_storage_mode::sidecar ? append_path : std::filesystem::path{};
    const auto encoded_header = encode_header(replacement);
    std::unique_ptr<file_stream> next_writer;
    if (mode == journal_storage_mode::sidecar) {
      auto companion = replacement;
      companion.kind = sidecar_file_kind;
      const auto companion_header = encode_header(companion);
      next_writer = std::make_unique<file_stream>(destination, file_open_mode::create);
      write_stream_bytes(*next_writer, companion_header);
      next_writer->sync();
      sync_parent_directory(destination);
      event(journal_io_event::after_sidecar_flush);
      file_stream output{staging, file_open_mode::create};
      write_stream_bytes(output, encoded_header);
      write_stream_bytes(output, frame.header);
      write_stream_bytes(output, envelope);
      write_stream_bytes(output, frame.footer);
      output.sync();
    } else {
      // Keep the prepared handle open across replacement; subsequent appends use this exact generation.
      next_writer = std::make_unique<file_stream>(staging, file_open_mode::create);
      write_stream_bytes(*next_writer, encoded_header);
      write_stream_bytes(*next_writer, frame.header);
      write_stream_bytes(*next_writer, envelope);
      write_stream_bytes(*next_writer, frame.footer);
      next_writer->sync();
    }
    event(journal_io_event::after_base_flush);
    event(journal_io_event::before_replace);
    // Windows replacement requires closing the old destination handle. The stable document lock
    // remains held; every failure after this point fences the store until reopening.
    if (mode == journal_storage_mode::appended) {
      writer.reset();
    }
    try {
      publish_file(staging, path, existing);
      event(journal_io_event::after_replace);
      sync_publication(staging, path);
      event(journal_io_event::after_publish);
    } catch (...) {
      fenced = true;
      throw nested_journal_error{"Managed full Save outcome is indeterminate"};
    }
    // All throwing preparation is complete before these publication assignments.
    header = replacement;
    sequence = replacement_sequence;
    append_path.swap(destination);
    writer.swap(next_writer);
    delta.swap(next_delta);
    replay_bytes = envelope.size();
    digest = frame.digest;
    append_offset = mode == journal_storage_mode::appended
                        ? file_header_bytes + frame_overhead_bytes + envelope.size()
                        : file_header_bytes;
    // Cleanup is optional: a failure here cannot turn a durable Save into a failed operation.
    try {
      event(journal_io_event::before_cleanup);
      if (!old_sidecar.empty()) {
        std::error_code ignored;
        native_remove(old_sidecar, ignored);
      }
    } catch (...) {
    }
  }

  // Stage a data-only file and sidecar before publishing a recoverable two-file decision.
  void replace_document(const bytes& envelope, journal_storage_mode mode, bool existing,
                         bool advance_sequence, std::span<const std::uint8_t> document,
                         std::span<const std::uint8_t> origin, bool prepare = false);

  // Reuse ordinary native publication after either synchronous staging or host confirmation.
  void publish_document(journal_document_manifest next_manifest,
      std::unique_ptr<journal_delta_base> next_delta, std::unique_ptr<file_stream> next_writer,
      std::size_t checkpoint_bytes, std::size_t envelope_bytes, bool existing,
      bool preserve_document, bool confirmed = false);

  // Validate a preparation's old activation and proposed metadata before exposing staged bytes.
  journal_prepared_save read_preparation(const journal_document_manifest& proposed) const;
};

// Bind a new managed checkpoint to exactly the document bytes selected by its durable manifest.
void file_journal::implementation::replace_document(
    const bytes& envelope, journal_storage_mode mode, bool existing, bool advance_sequence,
    std::span<const std::uint8_t> document, std::span<const std::uint8_t> origin, bool prepare) {
  if (mode != journal_storage_mode::sidecar || document.empty()) {
    throw std::invalid_argument{"Document codecs require a sidecar and a canonical root"};
  }
  const auto& codec = *options.document_codec;
  if (origin.size() > options.max_origin_bytes) {
    throw std::length_error{"Managed journal origin budget exceeded"};
  }
  if (codec.format.empty() || !codec.encode || !codec.decode) {
    throw std::invalid_argument{"Incomplete journal document codec"};
  }
  if (existing && !document_matches(manifest, read_file(path, options))) {
    throw std::invalid_argument{"Saved journal document changed under its writer lock"};
  }
  check_replay(0, envelope.size());
  if (envelope.size() > options.max_record_bytes) {
    throw std::length_error{"Managed document checkpoint budget exceeded"};
  }
  if (advance_sequence && sequence == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"Managed journal sequence exhausted"};
  }
  const bool attach_existing = !existing && native_exists(path);
  const bool preserve_document = attach_existing || advance_sequence;
  auto document_bytes = preserve_document ? read_file(path, options) : codec.encode(document);
  if (document_bytes.size() > options.max_file_bytes) {
    throw std::length_error{"Managed document file budget exceeded"};
  }
  const auto roundtrip = codec.decode(document_bytes);
  if (roundtrip.size() > options.max_record_bytes ||
      roundtrip.size() != document.size() ||
      !std::equal(roundtrip.begin(), roundtrip.end(), document.begin())) {
    throw std::invalid_argument{"Journal document codec does not preserve the canonical model"};
  }
  auto next_delta = std::make_unique<journal_delta_base>(bytes{document.begin(), document.end()});
  const auto checkpoint = next_delta->encode({envelope}, options.max_record_bytes);
  check_size(file_header_bytes, checkpoint.size());
  const auto replacement_sequence = sequence + static_cast<std::uint64_t>(advance_sequence);
  const auto frame = encode_frame({checkpoint}, replacement_sequence, 0, options);
  std::random_device entropy;
  std::uniform_int_distribution<std::uint64_t> word;
  file_header replacement{mode, base_file_kind, word(entropy), word(entropy), replacement_sequence,
                          frame.digest, differential_record_version};
  if (replacement.generation_high == 0 && replacement.generation_low == 0) {
    replacement.generation_low = 1;
  }
  auto destination = companion_path(replacement);
  const auto staging = staging_path(replacement);
  auto manifest_staging = staging;
  manifest_staging += ".manifest";
  const auto encoded_header = encode_header(replacement);
  journal_document_manifest next_manifest;
  std::copy(encoded_header.begin(), encoded_header.end(), next_manifest.header.begin());
  next_manifest.document_bytes = document_bytes.size();
  next_manifest.document_digest = rohit::make_digest<rohit::digest_algorithm::sha256>(document_bytes);
  next_manifest.codec = codec.format;
  next_manifest.origin.assign(origin.begin(), origin.end());
  next_manifest.confirmation = manifest.confirmation;
  if (prepare) {
    next_manifest.confirmation.clear();
    const auto proposed_bytes = encode_document_manifest(next_manifest, options.max_file_bytes,
                                                         options.max_origin_bytes);
    auto& token = next_manifest.confirmation;
    token.reserve(save_token_bytes);
    put_word(token, save_token_magic);
    put_word(token, 1);
    token.insert(token.end(), manifest.header.begin(), manifest.header.end());
    put_word(token, sequence);
    put_word(token, digest);
    const auto binding = make_digest<digest_algorithm::sha256>(proposed_bytes);
    token.insert(token.end(), binding.begin(), binding.end());
    put_word(token, next_manifest.document_bytes);
    token.insert(token.end(), next_manifest.document_digest.begin(), next_manifest.document_digest.end());
    put_word(token, checksum(token));
  }
  const auto manifest_bytes = encode_document_manifest(next_manifest, options.max_file_bytes,
                                                       options.max_origin_bytes);
  auto companion = replacement;
  companion.kind = sidecar_file_kind;
  const auto companion_header = encode_header(companion);
  auto next_writer = std::make_unique<file_stream>(destination, file_open_mode::create);
  write_stream_bytes(*next_writer, companion_header);
  write_frame(*next_writer, frame, {checkpoint}, {});
  next_writer->sync();
  sync_parent_directory(destination);
  event(journal_io_event::after_sidecar_flush);
  if (!preserve_document) {
    file_stream output{staging, file_open_mode::create};
    write_stream_bytes(output, document_bytes);
    output.sync();
  }
  {
    file_stream output{manifest_staging, file_open_mode::create};
    write_stream_bytes(output, manifest_bytes);
    output.sync();
  }
  event(journal_io_event::after_base_flush);
  if (prepare) {
    event(journal_io_event::before_prepare_publish);
    try {
      publish_file(manifest_staging, prepared_path(), false);
      sync_parent_directory(prepared_path());
      event(journal_io_event::after_prepare_publish);
    } catch (...) {
      fenced = true;
      throw nested_journal_error{"Managed Save preparation outcome is indeterminate"};
    }
    preparation.emplace(std::move(next_manifest));
    return;
  }
  publish_document(std::move(next_manifest), std::move(next_delta), std::move(next_writer),
                   checkpoint.size(), envelope.size(), existing, preserve_document);
}

// The pending manifest is the existing local commit decision; the unresolved intent survives it.
void file_journal::implementation::publish_document(journal_document_manifest next_manifest,
    std::unique_ptr<journal_delta_base> next_delta, std::unique_ptr<file_stream> next_writer,
    std::size_t checkpoint_bytes, std::size_t envelope_bytes, bool existing,
    bool preserve_document, bool confirmed) {
  const auto replacement = decode_header(next_manifest.header, base_file_kind);
  auto destination = companion_path(replacement);
  const auto staging = staging_path(replacement);
  auto manifest_staging = staging;
  manifest_staging += ".manifest";
  const auto old_sidecar = append_path;
  event(journal_io_event::before_replace);
  try {
    // The pending manifest is a durable recovery decision. A matching new document may be selected
    // after any later interruption, including a metadata-only Save whose document bytes are equal.
    publish_file(manifest_staging, manifest_path(true), false);
    sync_parent_directory(manifest_path(true));
    if (confirmed) { event(journal_io_event::after_confirm_decision); }
    if (!preserve_document) {
      publish_file(staging, path, native_exists(path));
      sync_publication(staging, path);
    }
    event(journal_io_event::after_replace);
    publish_file(manifest_path(true), manifest_path(), existing);
    sync_parent_directory(manifest_path());
    event(journal_io_event::after_publish);
  } catch (...) {
    fenced = true;
    throw nested_journal_error{"Managed document Save outcome is indeterminate"};
  }
  header = replacement;
  sequence = replacement.sequence;
  digest = replacement.base_digest;
  append_offset = file_header_bytes + frame_overhead_bytes + checkpoint_bytes;
  append_path.swap(destination);
  writer.swap(next_writer);
  delta.swap(next_delta);
  replay_bytes = envelope_bytes;
  std::swap(manifest, next_manifest);
  preparation.reset();
  try {
    event(journal_io_event::before_cleanup);
    if (!old_sidecar.empty()) {
      std::error_code ignored;
      native_remove(old_sidecar, ignored);
    }
    if (confirmed) {
      native_remove(prepared_path());
      sync_parent_directory(prepared_path());
    }
  } catch (...) {
  }
}

// Resolve the path once and keep the lock file stable across all base-file replacements.
file_journal::file_journal(const std::filesystem::path& path, journal_options options)
    : state_{std::make_unique<implementation>()} {
  if (path.empty() || options.max_record_bytes == 0 ||
      options.max_replay_bytes == 0 ||
      options.max_file_bytes < file_header_bytes + frame_overhead_bytes ||
      options.origin.size() > options.max_origin_bytes ||
      (!options.document_codec && !options.origin.empty())) {
    throw std::invalid_argument{"Invalid managed journal path or limits"};
  }
  state_->path = rohit::detail::canonical_file_path(path);
  state_->options = std::move(options);
  auto lock_path = state_->path;
  lock_path += ".lock";
  auto legacy_manifest = state_->path;
  legacy_manifest += journal_directory_suffix;
  auto legacy_pending = legacy_manifest;
  legacy_pending += ".pending";
  const auto directory = journal_directory(state_->path);
  const bool directory_exists = native_exists(directory);
  const bool legacy_metadata = native_exists(lock_path) ||
      native_exists(legacy_manifest) || native_exists(legacy_pending);
  if (directory_exists && legacy_metadata) {
    throw std::invalid_argument{"Conflicting directory and legacy flat journal layouts"};
  }
  // Old native containers may have been copied without their lock. Keep their existing
  // flat generation contract; data-only documents are recognized by their manifests.
  const bool legacy_native = !directory_exists && !state_->options.document_codec &&
      native_exists(state_->path);
  if (!legacy_metadata && !legacy_native) {
    prepare_journal_directory(directory);
    state_->auxiliary_directory = directory;
    lock_path = directory / journal_lock_name;
  }
  state_->lock = std::make_unique<file_stream>(lock_path, file_open_mode::lock);
}

// Out-of-line destruction hides platform handles from public headers.
file_journal::~file_journal() = default;

// Create a new baseline under exclusive ownership without replacing an existing document.
std::unique_ptr<file_journal> file_journal::create(const std::filesystem::path& path,
                                                   journal_storage_mode mode, const bytes& envelope,
                                                   journal_options options, bool& indeterminate,
                                                   std::span<const std::uint8_t> document) {
  auto result = std::unique_ptr<file_journal>{new file_journal{path, std::move(options)}};
  if ((!result->state_->options.document_codec && native_exists(result->state_->path)) ||
      (result->state_->options.document_codec &&
       (native_exists(result->state_->manifest_path()) ||
        native_exists(result->state_->manifest_path(true))))) {
    throw std::invalid_argument{"Managed journal document already exists"};
  }
  try {
    result->state_->replace(envelope, mode, false, false, document);
  } catch (...) {
    // Preserve uncertainty even if allocating the richer exception itself failed.
    indeterminate = result->state_->fenced;
    throw;
  }
  return result;
}

// Validate physical frames and expand differential records before isolated managed replay.
std::unique_ptr<file_journal>
file_journal::recover(const std::filesystem::path& path, journal_options options,
                      const std::function<void(bool, std::span<const std::uint8_t>)>& consume) {
  // Creation provenance is never an authority for replacing recovered native metadata.
  options.origin.clear();
  auto result = std::unique_ptr<file_journal>{new file_journal{path, std::move(options)}};
  auto& state = *result->state_;
  auto input = read_file(state.path, state.options);
  const bool external = static_cast<bool>(state.options.document_codec);
  bytes document{};
  if (external) {
    const auto& codec = *state.options.document_codec;
    if (codec.format.empty() || !codec.decode || !codec.encode) {
      throw std::invalid_argument{"Incomplete journal document codec"};
    }
    std::optional<journal_document_manifest> saved;
    std::optional<journal_document_manifest> pending;
    if (native_exists(state.manifest_path())) {
      saved = read_manifest(state.manifest_path(), state.options);
    }
    if (native_exists(state.manifest_path(true))) {
      pending = read_manifest(state.manifest_path(true), state.options);
    }
    if (pending && document_matches(*pending, input)) {
      state.manifest = std::move(*pending);
      state.pending_manifest = true;
    } else if (saved && document_matches(*saved, input)) {
      state.manifest = std::move(*saved);
    } else {
      throw std::invalid_argument{"Journal document does not match its saved generation"};
    }
    if (state.manifest.codec != codec.format) {
      throw std::invalid_argument{"Journal document codec does not match its saved format"};
    }
    if (!state.manifest.confirmation.empty()) {
      validate_save_token(state.manifest.confirmation);
    }
    state.header = decode_header(state.manifest.header, base_file_kind);
    if (state.header.mode != journal_storage_mode::sidecar ||
        state.header.version != differential_record_version) {
      throw std::invalid_argument{"Invalid managed document journal mode or version"};
    }
    // Admit the complete selected companion before invoking application document decoding.
    document = std::move(input);
    state.append_path = state.companion_path(state.header);
    input = read_file(state.append_path, state.options);
    auto companion = decode_header(input, sidecar_file_kind);
    companion.kind = base_file_kind;
    if (encode_header(companion) != encode_header(state.header)) {
      throw std::invalid_argument{"Managed sidecar does not match its document generation"};
    }
  } else {
    state.header = decode_header(input, base_file_kind);
  }
  const auto baseline =
      read_file_frame(input, file_header_bytes, state.header.sequence, 0, state.options);
  if (!baseline || baseline->digest != state.header.base_digest) {
    throw std::invalid_argument{"Incomplete or mismatched managed journal base"};
  }
  if (external) {
    auto canonical = state.options.document_codec->decode(document);
    if (canonical.empty() || canonical.size() > state.options.max_record_bytes) {
      throw std::length_error{"Managed document canonical root budget exceeded"};
    }
    // The decoder's borrowed document span has expired; keep only canonical and companion bytes.
    bytes{}.swap(document);
    state.delta = std::make_unique<journal_delta_base>(std::move(canonical));
    const auto envelope = state.delta->decode(baseline->payload, state.options.max_record_bytes,
                                              state.options.max_replay_bytes);
    state.replay_bytes = envelope.size();
    consume(true, envelope);
  } else {
    if (state.header.version == differential_record_version) {
      state.check_replay(0, baseline->payload.size());
      state.replay_bytes = baseline->payload.size();
    }
    consume(true, baseline->payload);
    if (state.header.version == differential_record_version) {
      state.delta = std::make_unique<journal_delta_base>(
          bytes{baseline->payload.begin(), baseline->payload.end()});
    }
  }
  state.sequence = state.header.sequence;
  state.digest = baseline->digest;
  state.append_offset = baseline->end_offset;
  if (!external) {
    state.append_path = state.path;
  }
  if (!external && state.header.mode == journal_storage_mode::sidecar) {
    if (state.append_offset != input.size()) {
      throw std::invalid_argument{"Trailing bytes in managed sidecar base"};
    }
    state.append_path = state.companion_path(state.header);
    input = read_file(state.append_path, state.options);
    auto companion = decode_header(input, sidecar_file_kind);
    companion.kind = base_file_kind;
    if (encode_header(companion) != encode_header(state.header)) {
      throw std::invalid_argument{"Managed sidecar does not match its base generation"};
    }
    state.append_offset = file_header_bytes;
  }
  while (state.append_offset < input.size()) {
    if (state.sequence == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error{"Managed journal sequence exhausted"};
    }
    auto frame = read_file_frame(input, state.append_offset, state.sequence + 1, state.digest,
                                 state.options);
    if (!frame) {
      state.incomplete_tail = true;
      break;
    }
    ++state.sequence;
    state.digest = frame->digest;
    state.append_offset = frame->end_offset;
    if (state.delta) {
      const auto remaining = state.options.max_replay_bytes - state.replay_bytes;
      const auto record = state.delta->decode(frame->payload,
          state.options.max_record_bytes, remaining);
      state.replay_bytes += record.size();
      consume(false, record);
    } else {
      consume(false, frame->payload);
    }
  }
  if (external && native_exists(state.prepared_path())) {
    auto prepared = read_manifest(state.prepared_path(), state.options);
    validate_save_token(prepared.confirmation);
    if (!same_bytes(prepared.confirmation, state.manifest.confirmation)) {
      // Validate staging before the store admits any recovered state or repairs the old tail.
      state.read_preparation(prepared);
      state.preparation.emplace(std::move(prepared));
    }
  }
  state.recovering = true;
  return result;
}

// Flush recovered complete records too: a prior failed flush may have left them only in the OS cache.
void file_journal::finish_recovery() {
  auto& state = *state_;
  if (!state.recovering || state.fenced) {
    throw std::logic_error{"Managed journal is not awaiting recovery validation"};
  }
  try {
    auto writer = std::make_unique<file_stream>(state.append_path, file_open_mode::update);
    if (state.incomplete_tail) {
      state.event(journal_io_event::before_tail_repair);
      writer->truncate(state.append_offset);
      state.event(journal_io_event::after_tail_repair);
    } else {
      writer->seek(state.append_offset);
    }
    writer->sync();
    // A previous Save may have failed after rename but before directory flush.
    sync_parent_directory(state.path);
    if (!state.auxiliary_directory.empty()) { sync_parent_directory(state.append_path); }
    if (state.options.document_codec) {
      if (state.pending_manifest) {
        publish_file(state.manifest_path(true), state.manifest_path(),
                       native_exists(state.manifest_path()));
        sync_parent_directory(state.manifest_path());
      } else if (native_exists(state.manifest_path(true))) {
        native_remove(state.manifest_path(true));
        sync_parent_directory(state.manifest_path(true));
      }
      state.pending_manifest = false;
      if (!state.preparation && native_exists(state.prepared_path())) {
        native_remove(state.prepared_path());
        sync_parent_directory(state.prepared_path());
      }
    }
    state.writer.swap(writer);
    state.recovering = false;
  } catch (...) {
    state.fenced = true;
    throw nested_journal_error{"Managed tail repair must be recovered"};
  }
}

// Prepare sidecar differences before I/O; the appended path continues to borrow its original buffers.
void file_journal::append(std::span<const std::uint8_t> prefix, std::span<const std::uint8_t> label,
                          std::span<const std::uint8_t> snapshot) {
  auto& state = *state_;
  state.require_ready();
  if (state.sequence == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"Managed journal sequence exhausted"};
  }
  std::size_t logical_bytes{};
  if (state.delta) {
    for (const auto part : {prefix, label, snapshot}) {
      if (part.size() > state.options.max_record_bytes - logical_bytes) {
        throw std::length_error{"Managed journal logical record budget exceeded"};
      }
      logical_bytes += part.size();
    }
    state.check_replay(state.replay_bytes, logical_bytes);
  }
  const auto patch = state.delta ? state.delta->encode({prefix, label, snapshot},
                                                        state.options.max_record_bytes) : bytes{};
  const std::initializer_list<std::span<const std::uint8_t>> parts{
      state.delta ? std::span<const std::uint8_t>{patch} : prefix,
      state.delta ? std::span<const std::uint8_t>{} : label,
      state.delta ? std::span<const std::uint8_t>{} : snapshot};
  const auto frame = encode_frame(parts, state.sequence + 1, state.digest, state.options);
  state.check_size(state.append_offset, frame.payload_bytes);
  // The exclusive writer retains its positioned handle; edits do not reopen or seek the file.
  auto& output = *state.writer;
  state.event(journal_io_event::before_append);
  try {
    write_frame(output, frame, parts, state.options);
    output.sync();
    state.event(journal_io_event::after_flush);
  } catch (...) {
    state.fenced = true;
    throw nested_journal_error{"Managed journal commit outcome is indeterminate"};
  }
  ++state.sequence;
  state.digest = frame.digest;
  state.append_offset += frame_overhead_bytes + frame.payload_bytes;
  state.replay_bytes += logical_bytes;
}

// A synchronous full Save covers the current durable sequence and retains the complete envelope.
void file_journal::save(const bytes& envelope) {
  const auto document = state_->options.document_codec
                            ? std::span<const std::uint8_t>{state_->delta->bytes()}
                            : std::span<const std::uint8_t>{};
  state_->replace(envelope, state_->header.mode, true, false, document);
}

// Save the external data projection and its managed metadata as one recoverable generation.
void file_journal::save_document(const bytes& envelope, std::span<const std::uint8_t> document) {
  state_->replace(envelope, state_->header.mode, true, false, document);
}

// Keep projection work out of native and legacy journal Save paths.
bool file_journal::uses_document_codec() const noexcept {
  return static_cast<bool>(state_->options.document_codec);
}

// Metadata is already integrity-checked during admission and remains independent of root codecs.
bytes file_journal::origin() const {
  if (!state_->options.document_codec) {
    throw std::logic_error{"Origin metadata requires an external document journal"};
  }
  return state_->manifest.origin;
}

// A provenance change is a native full Save, including the complete retained checkpoint.
void file_journal::save_document_origin(const bytes& envelope,
    std::span<const std::uint8_t> document, std::span<const std::uint8_t> origin) {
  state_->require_ready();
  if (!state_->options.document_codec) {
    throw std::logic_error{"Origin metadata requires an external document journal"};
  }
  state_->replace_document(envelope, state_->header.mode, true, false, document, origin);
}

// Stage one complete external package only at explicit Save preparation, never per edit.
journal_prepared_save file_journal::prepare_document_save(const bytes& envelope,
    std::span<const std::uint8_t> document, std::span<const std::uint8_t> origin) {
  state_->require_ready();
  if (!state_->options.document_codec) {
    throw std::logic_error{"Prepared Save requires an external document journal"};
  }
  state_->replace_document(envelope, state_->header.mode, true, false, document, origin, true);
  return *prepared_save();
}

// Bind the expected old native activation and exact proposed bytes before returning host buffers.
journal_prepared_save file_journal::implementation::read_preparation(
    const journal_document_manifest& proposed) const {
  const auto& token = proposed.confirmation;
  validate_save_token(token);
  if (!same_bytes(std::span<const std::uint8_t>{token}.subspan(token_header_offset, file_header_bytes), manifest.header) ||
      get_word(token, token_sequence_offset / wire_word_bytes) != sequence ||
      get_word(token, token_record_digest_offset / wire_word_bytes) != digest ||
      proposed.codec != options.document_codec->format ||
      decode_header(proposed.header, base_file_kind).sequence != sequence) {
    throw std::invalid_argument{"Prepared Save does not match the current native activation"};
  }
  auto unconfirmed = proposed;
  unconfirmed.confirmation.clear();
  const auto encoded = encode_document_manifest(unconfirmed, options.max_file_bytes,
                                                 options.max_origin_bytes);
  const auto binding = make_digest<digest_algorithm::sha256>(encoded);
  if (!same_bytes(std::span<const std::uint8_t>{token}.subspan(token_manifest_digest_offset, binding.size()),
                  binding)) {
    throw std::invalid_argument{"Prepared Save metadata binding does not match"};
  }
  const auto selected = decode_header(proposed.header, base_file_kind);
  auto document = read_file(staging_path(selected), options);
  if (!document_matches(proposed, document) ||
      get_word(token, token_document_size_offset / wire_word_bytes) != document.size() ||
      !same_bytes(std::span<const std::uint8_t>{token}.subspan(token_document_digest_offset,
                    proposed.document_digest.size()), proposed.document_digest)) {
    throw std::invalid_argument{"Prepared Save document binding does not match"};
  }
  const auto input = read_file(companion_path(selected), options);
  auto companion = decode_header(input, sidecar_file_kind);
  companion.kind = base_file_kind;
  const auto frame = read_file_frame(input, file_header_bytes, selected.sequence, 0, options);
  if (encode_header(companion) != encode_header(selected) || !frame ||
      frame->digest != selected.base_digest || frame->end_offset != input.size()) {
    throw std::invalid_argument{"Prepared Save checkpoint does not match its generation"};
  }
  return {token, std::move(document), proposed.origin};
}

// Returning a preparation never confirms it, including after external success without an acknowledgement.
std::optional<journal_prepared_save> file_journal::prepared_save() const {
  if (state_->fenced || state_->recovering) {
    throw journal_indeterminate_error{"Recover the journal before inspecting prepared Save"};
  }
  if (!state_->preparation) { return {}; }
  return state_->read_preparation(*state_->preparation);
}

// Inspect only the durable preparation's admitted in-memory state.
bool file_journal::has_prepared_save() const noexcept {
  return state_->preparation.has_value();
}

// Host evidence attests external publication; Serializer verifies its binding and publishes locally.
bool file_journal::confirm_document_save(std::span<const std::uint8_t> token,
                                        const journal_publication_evidence& evidence) {
  auto& state = *state_;
  if (state.fenced || state.recovering) {
    throw journal_indeterminate_error{"Recover the journal before confirming Save"};
  }
  validate_save_token(token);
  if (!state.options.document_codec ||
      get_word(token, token_document_size_offset / wire_word_bytes) != evidence.document_bytes ||
      !same_bytes(token.subspan(token_document_digest_offset, evidence.document_digest.size()),
                  evidence.document_digest)) {
    throw std::invalid_argument{"Publication evidence does not match prepared Save"};
  }
  if (!state.preparation) {
    if (same_bytes(token, state.manifest.confirmation)) { return false; }
    throw std::invalid_argument{"No matching prepared Save"};
  }
  if (!same_bytes(token, state.preparation->confirmation)) {
    throw std::invalid_argument{"Confirmation token does not match prepared Save"};
  }
  const auto prepared = state.read_preparation(*state.preparation);
  if (!document_matches(state.manifest, read_file(state.path, state.options))) {
    throw std::invalid_argument{"Saved journal document changed during prepared Save"};
  }
  auto proposed = *state.preparation;
  const auto header = decode_header(proposed.header, base_file_kind);
  const auto input = read_file(state.companion_path(header), state.options);
  auto companion = decode_header(input, sidecar_file_kind);
  companion.kind = base_file_kind;
  const auto frame = read_file_frame(input, file_header_bytes, header.sequence, 0, state.options);
  if (encode_header(companion) != encode_header(header) || !frame ||
      frame->digest != header.base_digest || frame->end_offset != input.size()) {
    throw std::invalid_argument{"Prepared Save checkpoint does not match its generation"};
  }
  auto canonical = state.options.document_codec->decode(prepared.document);
  if (canonical.empty() || canonical.size() > state.options.max_record_bytes) {
    throw std::length_error{"Prepared Save canonical root budget exceeded"};
  }
  auto next_delta = std::make_unique<journal_delta_base>(std::move(canonical));
  const auto envelope = next_delta->decode(frame->payload, state.options.max_record_bytes,
                                           state.options.max_replay_bytes);
  const auto manifest_bytes = encode_document_manifest(proposed, state.options.max_file_bytes,
                                                       state.options.max_origin_bytes);
  auto staging = state.staging_path(header);
  staging += ".manifest";
  // This scratch file may survive a failed confirmation before the durable decision.
  {
    file_stream output{staging, native_exists(staging) ? file_open_mode::update
                                                              : file_open_mode::create};
    output.truncate(0);
    write_stream_bytes(output, manifest_bytes);
    output.sync();
  }
  auto next_writer = std::make_unique<file_stream>(state.companion_path(header), file_open_mode::update);
  next_writer->seek(frame->end_offset);
  state.event(journal_io_event::before_confirm);
  state.publish_document(std::move(proposed), std::move(next_delta), std::move(next_writer),
      frame->payload.size(), envelope.size(), true, false, true);
  return true;
}

// Decode the saved frame lazily, without replaying or retaining the unsaved suffix.
bytes file_journal::saved_envelope() const {
  const auto& state = *state_;
  state.require_ready();
  if (state.options.document_codec) {
    const auto document = read_file(state.path, state.options);
    if (!document_matches(state.manifest, document)) {
      throw std::invalid_argument{"Saved journal document changed under its writer lock"};
    }
    file_stream companion{state.append_path, file_open_mode::read_shared};
    if (companion.size() > state.options.max_file_bytes) {
      throw std::length_error{"Managed journal file budget exceeded"};
    }
    std::array<std::uint8_t, file_header_bytes> header_bytes{};
    read_stream_exact(companion, header_bytes);
    auto header = decode_header(header_bytes, sidecar_file_kind);
    header.kind = base_file_kind;
    if (encode_header(header) != encode_header(state.header)) {
      throw std::invalid_argument{"Saved document journal generation changed under its writer lock"};
    }
    const auto baseline = read_journal_frame(companion, header.sequence, 0, state.options);
    if (!baseline || baseline->digest != header.base_digest) {
      throw std::invalid_argument{"Incomplete saved document journal baseline"};
    }
    return state.delta->decode(baseline->payload(), state.options.max_record_bytes);
  }
  // The stable journal lock supplies synchronization; normal read sharing remains unchanged.
  file_stream input{state.path, file_open_mode::read_shared};
  const auto size = input.size();
  if (size > state.options.max_file_bytes) {
    throw std::length_error{"Managed journal file budget exceeded"};
  }
  if (size < file_header_bytes) {
    throw std::invalid_argument{"Incomplete managed journal header"};
  }
  std::array<std::uint8_t, file_header_bytes> header_bytes{};
  read_stream_exact(input, header_bytes);
  const auto header = decode_header(header_bytes, base_file_kind);
  if (header.mode != state.header.mode || header.sequence != state.header.sequence ||
      header.version != state.header.version ||
      header.generation_high != state.header.generation_high ||
      header.generation_low != state.header.generation_low ||
      header.base_digest != state.header.base_digest) {
    throw std::invalid_argument{"Saved journal generation changed under its writer lock"};
  }
  const auto baseline = read_journal_frame(input, header.sequence, 0, state.options);
  if (!baseline || baseline->digest != header.base_digest ||
      (header.mode == journal_storage_mode::sidecar &&
       (size < file_header_bytes + frame_overhead_bytes ||
        baseline->payload().size() != size - file_header_bytes - frame_overhead_bytes))) {
    throw std::invalid_argument{"Incomplete or mismatched saved journal baseline"};
  }
  return {baseline->payload().begin(), baseline->payload().end()};
}

// Restore saved content through the same atomic, fenced generation replacement as full Save.
void file_journal::discard(const bytes& envelope) {
  const auto document = state_->options.document_codec
                            ? std::span<const std::uint8_t>{state_->delta->bytes()}
                            : std::span<const std::uint8_t>{};
  state_->replace(envelope, state_->header.mode, true, true, document);
}

// Return the monotonically increasing durable operation position, independently of history cursors.
std::uint64_t file_journal::sequence() const noexcept {
  return state_->sequence;
}

// Retirement is terminal: all data deletion precedes releasing/unlinking the lock.
// Main bytes are verified under that lock and never removed or rewritten here.
bool file_journal::retire_document() {
  auto& state = *state_;
  state.require_ready();
  if (!state.options.document_codec || !state.options.allow_document_retirement || state.auxiliary_directory.empty() ||
      state.preparation || state.pending_manifest) { return false; }
  if (!document_matches(state.manifest, read_file(state.path, state.options))) {
    throw std::invalid_argument{"Saved document changed before journal retirement"};
  }
  const rohit::detail::native_file_path directory{state.auxiliary_directory};
  if (std::filesystem::symlink_status(directory.get()).type() != std::filesystem::file_type::directory) {
    throw std::invalid_argument{"Journal retirement requires an ordinary directory"};
  }
  std::vector<std::filesystem::path> companions;
  constexpr std::size_t maximum_retirement_entries = 4096;
  std::size_t entries{};
  for (const auto& entry : std::filesystem::directory_iterator{directory.get()}) {
    if (++entries > maximum_retirement_entries) {
      throw std::length_error{"Journal retirement entry limit exceeded"};
    }
    if (entry.symlink_status().type() != std::filesystem::file_type::regular) {
      throw std::invalid_argument{"Journal retirement preserves non-file entries for inspection"};
    }
    if (entry.path().filename() != journal_lock_name) { companions.push_back(entry.path()); }
  }
  // From this point no live mutation may resume, including after partial cleanup.
  state.fenced = true;
  state.writer.reset(); state.delta.reset();
  for (const auto& companion : companions) { native_remove(companion); }
  const auto lock = state.auxiliary_directory / journal_lock_name;
#ifdef _WIN32
  // Windows excludes every other opener while held and refuses deletion if a
  // replacement writer acquires the file immediately after release.
  state.lock.reset();
  std::error_code error;
  if (!native_remove(lock, error)) {
    if (error) { return true; } // A competing writer owns its remaining folder.
  }
#else
  // New openers validate inode identity after flock before accessing any data.
  native_remove(lock);
#endif
  std::error_code directory_error;
  native_remove(state.auxiliary_directory, directory_error); // Never recurse into a new writer's files.
  state.lock.reset();
  sync_parent_directory(state.auxiliary_directory);
  return true;
}

// A fenced adapter cannot resume writes through an in-memory retry.
bool file_journal::needs_recovery() const noexcept {
  return state_->fenced;
}

} // namespace

// Keep the concrete file backend out of the managed store's owned sink type.
std::unique_ptr<journal_sink> create_file_journal(const std::filesystem::path& path,
                                                  journal_storage_mode mode,
                                                  const std::vector<std::uint8_t>& envelope,
                                                  journal_options options, bool& indeterminate,
                                                  std::span<const std::uint8_t> document) {
  return file_journal::create(path, mode, envelope, std::move(options), indeterminate, document);
}

// Return the same sink interface after file-specific generation selection and stream replay.
std::unique_ptr<journal_sink>
recover_file_journal(const std::filesystem::path& path, journal_options options,
                     const std::function<void(bool, std::span<const std::uint8_t>)>& consume) {
  return file_journal::recover(path, std::move(options), consume);
}

} // namespace rohit::managed::detail
