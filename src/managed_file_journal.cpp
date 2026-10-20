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

#include <array>
#include <algorithm>
#include <exception>
#include <limits>
#include <random>
#include <string>
#include <system_error>
#include <utility>

namespace rohit::managed::detail {
namespace {
using bytes = std::vector<std::uint8_t>;
constexpr std::size_t file_header_bytes = 9 * wire_word_bytes;
constexpr std::uint64_t file_magic = 0x314c4e4a5a5253;
constexpr std::uint64_t record_version = 2;
constexpr std::uint64_t differential_record_version = 3;
constexpr std::uint64_t base_file_kind = 1;
constexpr std::uint64_t sidecar_file_kind = 2;

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
bytes read_file(const std::filesystem::path& path, const journal_options& options) {
  file_stream input{path, file_open_mode::read};
  const auto size = input.size();
  if (size > options.max_file_bytes) {
    throw std::length_error{"Managed journal file budget exceeded"};
  }
  bytes result(static_cast<std::size_t>(size));
  read_stream_exact(input, result);
  return result;
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
  // Read only the selected full-Save baseline while retaining the existing writer lock.
  bytes saved_envelope() const override;
  // Publish a restored envelope with a fresh operation sequence.
  void discard(const bytes& envelope) override;
  // Return the last acknowledged operation sequence.
  std::uint64_t sequence() const noexcept override;
  // Expose fencing after an uncertain durable operation.
  bool needs_recovery() const noexcept override;
};

struct file_journal::implementation {
  std::filesystem::path path{};
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

  // Use a stable side-channel manifest without adding managed data to application document bytes.
  std::filesystem::path manifest_path(bool pending = false) const {
    auto result = path;
    result += pending ? ".journal.pending" : ".journal";
    return result;
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
      replace_document(envelope, mode, existing, advance_sequence, document);
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
    const auto staging = generation_path(path, replacement, ".pending-");
    auto destination = mode == journal_storage_mode::appended
                           ? path
                           : generation_path(path, replacement, ".journal-");
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
      sync_parent_directory(path);
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
        std::filesystem::remove(old_sidecar, ignored);
      }
    } catch (...) {
    }
  }

  // Stage a data-only file and sidecar before publishing a recoverable two-file decision.
  void replace_document(const bytes& envelope, journal_storage_mode mode, bool existing,
                         bool advance_sequence, std::span<const std::uint8_t> document);
};

// Bind a new managed checkpoint to exactly the document bytes selected by its durable manifest.
void file_journal::implementation::replace_document(
    const bytes& envelope, journal_storage_mode mode, bool existing, bool advance_sequence,
    std::span<const std::uint8_t> document) {
  if (mode != journal_storage_mode::sidecar || document.empty()) {
    throw std::invalid_argument{"Document codecs require a sidecar and a canonical root"};
  }
  const auto& codec = *options.document_codec;
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
  const bool attach_existing = !existing && std::filesystem::exists(path);
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
  auto destination = generation_path(path, replacement, ".journal-");
  const auto staging = generation_path(path, replacement, ".pending-");
  auto manifest_staging = staging;
  manifest_staging += ".manifest";
  const auto old_sidecar = append_path;
  const auto encoded_header = encode_header(replacement);
  journal_document_manifest next_manifest;
  std::copy(encoded_header.begin(), encoded_header.end(), next_manifest.header.begin());
  next_manifest.document_bytes = document_bytes.size();
  next_manifest.document_digest = rohit::make_digest<rohit::digest_algorithm::sha256>(document_bytes);
  next_manifest.codec = codec.format;
  const auto manifest_bytes = encode_document_manifest(next_manifest, options.max_file_bytes);
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
  event(journal_io_event::before_replace);
  try {
    // The pending manifest is a durable recovery decision. A matching new document may be selected
    // after any later interruption, including a metadata-only Save whose document bytes are equal.
    publish_file(manifest_staging, manifest_path(true), false);
    sync_parent_directory(path);
    if (!preserve_document) {
      publish_file(staging, path, std::filesystem::exists(path));
      sync_parent_directory(path);
    }
    event(journal_io_event::after_replace);
    publish_file(manifest_path(true), manifest_path(), existing);
    sync_parent_directory(path);
    event(journal_io_event::after_publish);
  } catch (...) {
    fenced = true;
    throw nested_journal_error{"Managed document Save outcome is indeterminate"};
  }
  header = replacement;
  sequence = replacement_sequence;
  digest = frame.digest;
  append_offset = file_header_bytes + frame_overhead_bytes + checkpoint.size();
  append_path.swap(destination);
  writer.swap(next_writer);
  delta.swap(next_delta);
  replay_bytes = envelope.size();
  std::swap(manifest, next_manifest);
  try {
    event(journal_io_event::before_cleanup);
    if (!old_sidecar.empty()) {
      std::error_code ignored;
      std::filesystem::remove(old_sidecar, ignored);
    }
  } catch (...) {
  }
}

// Resolve the path once and keep the lock file stable across all base-file replacements.
file_journal::file_journal(const std::filesystem::path& path, journal_options options)
    : state_{std::make_unique<implementation>()} {
  if (path.empty() || options.max_record_bytes == 0 ||
      options.max_replay_bytes == 0 ||
      options.max_file_bytes < file_header_bytes + frame_overhead_bytes) {
    throw std::invalid_argument{"Invalid managed journal path or limits"};
  }
  state_->path = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
  state_->options = std::move(options);
  auto lock_path = state_->path;
  lock_path += ".lock";
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
  if ((!result->state_->options.document_codec && std::filesystem::exists(result->state_->path)) ||
      (result->state_->options.document_codec &&
       (std::filesystem::exists(result->state_->manifest_path()) ||
        std::filesystem::exists(result->state_->manifest_path(true))))) {
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
    if (std::filesystem::exists(state.manifest_path())) {
      saved = decode_document_manifest(read_file(state.manifest_path(), state.options),
                                        state.options.max_file_bytes);
    }
    if (std::filesystem::exists(state.manifest_path(true))) {
      pending = decode_document_manifest(read_file(state.manifest_path(true), state.options),
                                          state.options.max_file_bytes);
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
    state.header = decode_header(state.manifest.header, base_file_kind);
    if (state.header.mode != journal_storage_mode::sidecar ||
        state.header.version != differential_record_version) {
      throw std::invalid_argument{"Invalid managed document journal mode or version"};
    }
    // Admit the complete selected companion before invoking application document decoding.
    document = std::move(input);
    state.append_path = generation_path(state.path, state.header, ".journal-");
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
    state.append_path = generation_path(state.path, state.header, ".journal-");
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
    if (state.options.document_codec) {
      if (state.pending_manifest) {
        publish_file(state.manifest_path(true), state.manifest_path(),
                       std::filesystem::exists(state.manifest_path()));
        sync_parent_directory(state.path);
      } else if (std::filesystem::exists(state.manifest_path(true))) {
        std::filesystem::remove(state.manifest_path(true));
        sync_parent_directory(state.path);
      }
      state.pending_manifest = false;
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
