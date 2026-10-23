// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "native_file_path.hpp"
#include <rohit/cache.hpp>
#include <rohit/file_stream.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <stdexcept>
#include <thread>
#include <tuple>

namespace rohit::cache {
namespace {
constexpr std::uint64_t record_magic = 0x3145484341435253ULL;
constexpr std::uint64_t record_version = 1;
constexpr std::size_t word_bytes = 8; // Version-one little-endian wire words, independent of ABI.
constexpr std::size_t key_words = 3;
constexpr std::size_t key_hashes = 4;
constexpr std::size_t key_bytes = key_words * word_bytes + key_hashes * fingerprint{}.size();
constexpr std::size_t header_words = 4;
constexpr std::size_t header_bytes = header_words * word_bytes + key_bytes;
constexpr std::size_t overhead_bytes = header_bytes + fingerprint{}.size();
constexpr std::string_view entry_suffix = ".srcache";

// Encode specified wire words, independent of host layout/endian.
void append_word(bytes& output, std::uint64_t value) {
  for (std::size_t index{}; index < word_bytes; ++index) {
    output.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
  }
}
// Read one previously bounded wire word.
std::uint64_t read_word(std::span<const std::uint8_t> input, std::size_t offset) {
  if (offset > input.size() || word_bytes > input.size() - offset) {
    throw std::invalid_argument{"Truncated cache record"};
  }
  std::uint64_t result{};
  for (std::size_t index{}; index < word_bytes; ++index) {
    result |= static_cast<std::uint64_t>(input[offset + index]) << (index * 8);
  }
  return result;
}
// Encode every source discriminator into one fixed-size key.
bytes encode_key(const key& identity) {
  bytes output;
  output.reserve(key_bytes);
  append_word(output, identity.document_high);
  append_word(output, identity.document_low);
  append_word(output, identity.entity);
  for (const auto* hash :
       {&identity.declaration, &identity.source, &identity.recipe, &identity.parameters}) {
    output.insert(output.end(), hash->begin(), hash->end());
  }
  return output;
}
// Decode key bytes only after bounding their complete fixed extent.
key decode_key(std::span<const std::uint8_t> input) {
  if (input.size() != key_bytes) {
    throw std::invalid_argument{"Invalid cache key extent"};
  }
  key result;
  result.document_high = read_word(input, 0);
  result.document_low = read_word(input, word_bytes);
  result.entity = read_word(input, 2 * word_bytes);
  std::size_t offset = key_words * word_bytes;
  for (auto* hash : {&result.declaration, &result.source, &result.recipe, &result.parameters}) {
    std::copy_n(input.begin() + offset, hash->size(), hash->begin());
    offset += hash->size();
  }
  return result;
}
// Prevent accidental use of an unnamespaced document or anonymous recipe.
void validate_key(const key& identity) {
  if (identity.document_high == 0 && identity.document_low == 0) {
    throw std::invalid_argument{"Cache requires a document identity"};
  }
}
// Names are derived from fixed hashes, never application-provided paths.
std::string entry_name(const key& identity) {
  return digest_to_hex(make_digest<digest_algorithm::sha256>(encode_key(identity))) +
         std::string{entry_suffix};
}
// Recognize only this provider's reserved entry namespace.
bool owned_name(std::string_view name) {
  constexpr auto hex_bytes = fingerprint{}.size() * 2;
  return name.size() == hex_bytes + entry_suffix.size() && name.substr(hex_bytes) == entry_suffix &&
         std::all_of(name.begin(), name.begin() + hex_bytes,
                     [](char ch) { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); });
}
// Reject file/directory aliases before accessing provider-owned paths.
void require_ordinary(const std::filesystem::path& path, bool directory,
                      bool allow_missing = false) {
  const rohit::detail::native_file_path native{path};
  std::error_code error;
  const auto type = std::filesystem::symlink_status(native.get(), error).type();
  if (allow_missing && (type == std::filesystem::file_type::not_found ||
                        error == std::errc::no_such_file_or_directory)) {
    return;
  }
  if (error) {
    throw std::filesystem::filesystem_error{"Inspect cache path", path, error};
  }
  if (type !=
      (directory ? std::filesystem::file_type::directory : std::filesystem::file_type::regular)) {
    throw std::invalid_argument{"Cache paths must be ordinary files/directories"};
  }
}
// Write all bytes through the runtime's exact native byte stream.
void write_bytes(file_stream& output, std::span<const std::uint8_t> value) {
  output.write(reinterpret_cast<const char*>(value.data()),
               static_cast<std::streamsize>(value.size()));
}
// Encode a bounded complete record with an integrity digest covering metadata and payload.
bytes encode_entry(const key& identity, std::span<const std::uint8_t> value,
                   std::uint8_t priority) {
  if (value.size() >
      static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()) - overhead_bytes) {
    throw std::length_error{"Cache record extent overflow"};
  }
  // Allocate the checked final extent once; no dependent insert can grow or overflow the record.
  bytes output(overhead_bytes + value.size());
  const auto store_word = [&](std::size_t word, std::uint64_t number) {
    for (std::size_t index{}; index < word_bytes; ++index) {
      output[word * word_bytes + index] = static_cast<std::uint8_t>(number >> (index * 8));
    }
  };
  store_word(0, record_magic);
  store_word(1, record_version);
  store_word(2, priority);
  store_word(3, value.size());
  const auto encoded = encode_key(identity);
  std::copy(encoded.begin(), encoded.end(), output.begin() + header_words * word_bytes);
  std::copy(value.begin(), value.end(), output.begin() + header_bytes);
  const auto digest = make_digest<digest_algorithm::sha256>(
      std::span<const std::uint8_t>{output}.first(output.size() - fingerprint{}.size()));
  std::copy(digest.begin(), digest.end(), output.end() - digest.size());
  return output;
}
// Own validated bytes; malformed optional data never escapes as a ready result.
std::shared_ptr<const entry> read_entry(const std::filesystem::path& path, std::size_t max_bytes) {
  require_ordinary(path, false);
  file_stream input{path, file_open_mode::read};
  const auto size = input.size();
  if (size < overhead_bytes || size - overhead_bytes > max_bytes ||
      size > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
    throw std::length_error{"Cache record budget exceeded"};
  }
  bytes record(static_cast<std::size_t>(size));
  input.read(reinterpret_cast<char*>(record.data()), static_cast<std::streamsize>(record.size()));
  if (input.fail() || static_cast<std::size_t>(input.gcount()) != record.size() ||
      read_word(record, 0) != record_magic || read_word(record, word_bytes) != record_version ||
      read_word(record, 2 * word_bytes) > std::numeric_limits<std::uint8_t>::max() ||
      read_word(record, 3 * word_bytes) != record.size() - overhead_bytes) {
    throw std::invalid_argument{"Invalid cache record"};
  }
  const auto view = std::span<const std::uint8_t>{record};
  const auto digest =
      make_digest<digest_algorithm::sha256>(view.first(record.size() - fingerprint{}.size()));
  if (!std::equal(digest.begin(), digest.end(), record.end() - digest.size())) {
    throw std::invalid_argument{"Cache integrity mismatch"};
  }
  auto result = std::make_shared<entry>();
  result->identity = decode_key(view.subspan(header_words * word_bytes, key_bytes));
  validate_key(result->identity);
  if (entry_name(result->identity) != path.filename().string()) {
    throw std::invalid_argument{"Cache filename identity mismatch"};
  }
  result->priority = static_cast<std::uint8_t>(read_word(record, 2 * word_bytes));
  result->identity.declared_priority = result->priority;
  result->value.assign(record.begin() + header_bytes, record.end() - digest.size());
  return result;
}
} // namespace

// Hash all non-wire cache identities independently, retaining no borrowed application strings.
key make_key(std::uint64_t document_high, std::uint64_t document_low, std::uint64_t entity,
             descriptor declaration, fingerprint source, std::string_view recipe,
             std::span<const std::uint8_t> parameters) {
  if (declaration.identity.empty() || recipe.empty()) {
    throw std::invalid_argument{"Cache requires declaration and recipe identities"};
  }
  key result{document_high,
             document_low,
             entity,
             make_digest<digest_algorithm::sha256>(declaration.identity),
             source,
             make_digest<digest_algorithm::sha256>(recipe),
             make_digest<digest_algorithm::sha256>(parameters),
             declaration.priority};
  validate_key(result);
  return result;
}

// Preserve native Unicode spelling and the entire document filename in the companion name.
std::filesystem::path companion_directory(const std::filesystem::path& document) {
  if (document.empty() || document.filename().empty()) {
    throw std::invalid_argument{"Empty document path"};
  }
  std::filesystem::path name{"."};
  name += document.filename().native();
  name += ".serializer";
  return document.parent_path() / name;
}

struct directory_provider::implementation {
  std::filesystem::path directory{};
  directory_options options{};
  std::unique_ptr<file_stream> lock{};
  std::mutex mutex{};
  std::map<key, std::weak_ptr<const entry>> pins{};
  std::set<key> invalidated{};

  // Invoke only explicit qualification hooks while storage ownership is held.
  void event(io_event point) const {
    if (options.fault_injector) {
      options.fault_injector(point);
    }
  }
  // Resolve only provider-derived filenames below the chosen cache root.
  std::filesystem::path path(const key& identity) const {
    return directory / entry_name(identity);
  }
  // Check an owning read pin without retaining the payload unnecessarily.
  bool pinned(const key& identity) {
    const auto found = pins.find(identity);
    if (found == pins.end()) {
      return false;
    }
    if (!found->second.expired()) {
      return true;
    }
    pins.erase(found);
    return false;
  }
  // Remove exactly one reserved ordinary entry; never follow a symlink into another namespace.
  bool remove(const std::filesystem::path& target) {
    require_ordinary(target, false, true);
    event(io_event::before_remove);
    const rohit::detail::native_file_path native{target};
    const bool removed = std::filesystem::remove(native.get());
    if (removed) {
      sync_parent_directory(target);
    }
    return removed;
  }
  // Validate/purge under exclusive ownership; payload validation is bounded per entry and scan.
  std::vector<info> scan() {
    std::vector<info> result;
    const rohit::detail::native_file_path native{directory};
    std::size_t scanned{};
    std::uint64_t inspected_bytes{};
    for (const auto& item : std::filesystem::directory_iterator(native.get())) {
      if (++scanned > options.max_scan_entries) {
        throw std::length_error{"Cache enumeration budget exceeded"};
      }
      if (!owned_name(item.path().filename().string())) {
        continue;
      }
      if (item.symlink_status().type() == std::filesystem::file_type::regular) {
        const auto size = item.file_size();
        if (size > options.max_total_bytes -
                       std::min<std::uint64_t>(inspected_bytes, options.max_total_bytes)) {
          throw std::length_error{"Cache cumulative enumeration byte budget exceeded"};
        }
        inspected_bytes += size;
      }
      std::shared_ptr<const entry> selected;
      try {
        selected = read_entry(item.path(), options.max_entry_bytes);
      } catch (const std::exception&) {
        // A corrupt optional file is a miss, but aliases are never deleted through this path.
        if (item.symlink_status().type() == std::filesystem::file_type::regular) {
          remove(item.path());
        }
        continue;
      }
      const bool held = pinned(selected->identity);
      if (invalidated.contains(selected->identity) && !held) {
        remove(item.path());
        invalidated.erase(selected->identity);
        continue;
      }
      result.push_back(
          {selected->identity, selected->priority,
           static_cast<std::int64_t>(item.last_write_time().time_since_epoch().count()),
           item.file_size(), held});
    }
    return result;
  }
  // Free eligible bytes with deterministic policy and actual available-space observations.
  reclaim_result reclaim(std::uint64_t target_bytes) {
    reclaim_result result;
    const rohit::detail::native_file_path native{directory};
    const auto before = std::filesystem::space(native.get()).available;
    try {
      auto entries = scan();
      std::sort(entries.begin(), entries.end(), [](const info& left, const info& right) {
        return std::tie(left.priority, left.last_access, left.identity) <
               std::tie(right.priority, right.last_access, right.identity);
      });
      for (const auto& item : entries) {
        if (item.pinned) {
          result.pinned_bytes += item.stored_bytes;
          continue;
        }
        const auto available = std::filesystem::space(native.get()).available;
        // Unrelated filesystem activity must not satisfy a cache cleanup request before this
        // provider has actually retired the requested amount of its own eligible file data.
        if (result.removed_file_bytes >= target_bytes && available >= before &&
            available - before >= target_bytes) {
          break;
        }
        if (remove(path(item.identity))) {
          result.removed_file_bytes += item.stored_bytes;
          ++result.removed_entries;
        }
      }
    } catch (...) {
      result.error = std::current_exception();
    }
    const auto after = std::filesystem::space(native.get()).available;
    result.reclaimed_bytes = after > before ? after - before : 0;
    result.shortfall_bytes =
        target_bytes > result.reclaimed_bytes ? target_bytes - result.reclaimed_bytes : 0;
    return result;
  }
};

// Acquire one provider-wide writer lock before inspecting or cleaning any cache entries.
directory_provider::directory_provider(std::filesystem::path directory, directory_options options)
    : state_{std::make_unique<implementation>()} {
  if (directory.empty() || options.max_entry_bytes == 0 || options.max_entries == 0 ||
      options.max_scan_entries < options.max_entries ||
      options.max_entry_bytes > options.max_total_bytes ||
      options.max_total_bytes >
          static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()) - overhead_bytes) {
    throw std::invalid_argument{"Invalid cache directory or limits"};
  }
  state_->directory = std::filesystem::absolute(directory).lexically_normal();
  state_->options = std::move(options);
  // The application owns the provider's ancestors. Reject aliases along the complete path.
  for (auto current = state_->directory; !current.empty() && current != current.root_path();
       current = current.parent_path()) {
    require_ordinary(current, true, true);
  }
  const rohit::detail::native_file_path native{state_->directory};
  std::filesystem::create_directories(native.get());
  require_ordinary(state_->directory, true);
  const auto lock_path = state_->directory / "cache.lock";
  require_ordinary(lock_path, false, true);
  state_->lock = std::make_unique<file_stream>(lock_path, file_open_mode::lock);
  sync_parent_directory(lock_path);
  // Only this provider's staging spelling is reserved. An exclusive cache lock proves no live
  // publication can still own such a file; other temporary names remain application-owned.
  std::size_t inspected{};
  for (const auto& item : std::filesystem::directory_iterator(native.get())) {
    if (++inspected > state_->options.max_scan_entries) {
      throw std::length_error{"Cache staging scan budget exceeded"};
    }
    const auto name = item.path().filename().string();
    const auto dash = name.find('-');
    if (dash == std::string::npos || !owned_name(std::string_view{name}.substr(0, dash)) ||
        !name.ends_with(".tmp")) {
      continue;
    }
    const auto suffix = std::string_view{name}.substr(dash + 1, name.size() - dash - 1 - 4);
    if (suffix.empty() || !std::all_of(suffix.begin(), suffix.end(), [](char ch) {
          return (ch >= '0' && ch <= '9') || ch == '-';
        })) {
      continue;
    }
    state_->remove(item.path());
  }
}

// Out-of-line destruction releases native file ownership without purging the domain.
directory_provider::~directory_provider() = default;

// Cache corruption is an optional miss; provider access/deletion failures remain explicit errors.
std::shared_ptr<const entry> directory_provider::lookup(const key& identity) {
  validate_key(identity);
  std::lock_guard guard{state_->mutex};
  if (state_->invalidated.contains(identity)) {
    return {};
  }
  const auto existing = state_->pins.find(identity);
  if (existing != state_->pins.end()) {
    if (auto pin = existing->second.lock()) {
      const auto selected_path = state_->path(identity);
      const rohit::detail::native_file_path native{selected_path};
      std::filesystem::last_write_time(native.get(), std::filesystem::file_time_type::clock::now());
      return pin;
    }
    state_->pins.erase(existing);
  }
  const auto path = state_->path(identity);
  const rohit::detail::native_file_path native{path};
  if (!std::filesystem::exists(native.get())) {
    return {};
  }
  std::shared_ptr<const entry> selected;
  try {
    selected = read_entry(path, state_->options.max_entry_bytes);
  } catch (const std::exception&) {
    state_->remove(path);
    return {};
  }
  if (selected->identity != identity) {
    state_->remove(path);
    return {};
  }
  std::filesystem::last_write_time(native.get(), std::filesystem::file_time_type::clock::now());
  state_->pins[identity] = selected;
  return selected;
}

// Reserve both replacement and staging space before writing; old bytes remain selected on failure.
void directory_provider::publish(const key& identity, std::span<const std::uint8_t> value,
                                 std::optional<std::uint8_t> priority) {
  validate_key(identity);
  std::lock_guard guard{state_->mutex};
  if (value.size() > state_->options.max_entry_bytes) {
    throw std::length_error{"Cache entry budget exceeded"};
  }
  auto records = state_->scan();
  std::uint64_t used{};
  bool replacing{};
  for (const auto& record : records) {
    if (record.stored_bytes > state_->options.max_total_bytes -
                                  std::min<std::uint64_t>(used, state_->options.max_total_bytes)) {
      used = state_->options.max_total_bytes;
      break;
    }
    used += record.stored_bytes;
    replacing = replacing || record.identity == identity;
  }
  const auto staged_bytes = overhead_bytes + value.size();
  const rohit::detail::native_file_path native_root{state_->directory};
  const auto available = std::filesystem::space(native_root.get()).available;
  if (used > state_->options.max_total_bytes ||
      staged_bytes > state_->options.max_total_bytes - used ||
      (!replacing && records.size() >= state_->options.max_entries) ||
      available < state_->options.reserve_free_bytes ||
      staged_bytes > available - state_->options.reserve_free_bytes) {
    throw std::length_error{"Cache storage admission denied; reclaim entries before retrying"};
  }
  auto encoded = encode_entry(identity, value, priority.value_or(identity.declared_priority));
  std::random_device entropy;
  const auto destination = state_->path(identity);
  auto staging = destination;
  staging += "-" + std::to_string(entropy()) + "-" + std::to_string(entropy()) + ".tmp";
  try {
    state_->event(io_event::before_stage);
    {
      file_stream output{staging, file_open_mode::create};
      write_bytes(output, encoded);
      output.sync();
    }
    state_->event(io_event::after_stage);
    state_->event(io_event::before_publish);
    require_ordinary(destination, false, true);
    publish_file(staging, destination, true);
    sync_parent_directory(destination);
    state_->pins.erase(identity);
    state_->invalidated.erase(identity);
    state_->event(io_event::after_publish);
  } catch (...) {
    const rohit::detail::native_file_path native{staging};
    std::error_code ignored;
    std::filesystem::remove(native.get(), ignored);
    throw;
  }
}

// Invalidation immediately blocks future lookup and removes disk bytes independently of owning pins.
bool directory_provider::invalidate(const key& identity) {
  validate_key(identity);
  std::lock_guard guard{state_->mutex};
  state_->invalidated.insert(identity);
  // Pins own decoded bytes, not open file handles. Explicit invalidation can remove disk bytes
  // immediately (including for revocation); ordinary priority reclamation still respects pins.
  state_->remove(state_->path(identity));
  state_->pins.erase(identity);
  state_->invalidated.erase(identity);
  return true;
}

// Serialize directory inspection with publication and reclamation.
std::vector<info> directory_provider::enumerate() {
  std::lock_guard guard{state_->mutex};
  return state_->scan();
}

// Disk priority and active disk-read pins are independent of service/in-memory retention.
reclaim_result directory_provider::reclaim(std::uint64_t target_bytes) {
  std::lock_guard guard{state_->mutex};
  return state_->reclaim(target_bytes);
}

struct service::implementation {
  struct slot {
    status state{status::absent};
    std::thread::id builder{};
    result completed{};
    bool invalidated{};
    std::uint64_t last_access{};
    std::condition_variable changed{};
  };
  std::shared_ptr<provider> storage{};
  std::size_t max_entries{};
  std::size_t max_resident_bytes{};
  std::uint64_t clock{};
  mutable std::mutex mutex{};
  std::map<key, std::shared_ptr<slot>> slots{};
  std::vector<std::weak_ptr<const entry>> residents{};

  // Include outstanding invalidated pins in admission; weak accounting never extends their lifetime.
  std::size_t retained_bytes() {
    std::size_t total{};
    for (auto item = residents.begin(); item != residents.end();) {
      if (const auto pin = item->lock()) {
        if (max_resident_bytes < sizeof(entry) ||
            pin->value.capacity() > max_resident_bytes - sizeof(entry)) {
          throw std::length_error{"Cache resident byte budget exceeded"};
        }
        const auto charge = sizeof(entry) + pin->value.capacity();
        if (charge > max_resident_bytes - std::min(total, max_resident_bytes)) {
          throw std::length_error{"Cache resident byte budget exceeded"};
        }
        total += charge;
        ++item;
      } else {
        item = residents.erase(item);
      }
    }
    return total;
  }
};

// A zero slot budget is an error, rather than an unbounded cache.
service::service(std::shared_ptr<provider> storage, std::size_t max_entries,
                 std::size_t max_resident_bytes)
    : state_{std::make_unique<implementation>()} {
  if (max_entries == 0 || max_resident_bytes == 0) {
    throw std::invalid_argument{"Cache service requires finite slot/byte budgets"};
  }
  state_->storage = std::move(storage);
  state_->max_entries = max_entries;
  state_->max_resident_bytes = max_resident_bytes;
}
// Callers join their tasks before destroying the service; no callbacks run during destruction.
service::~service() = default;

// Rebuild callbacks and validators never run under a service or native storage mutex.
result service::get_or_rebuild(const key& identity, const std::function<bytes()>& rebuild,
                               request_options options) {
  std::shared_ptr<implementation::slot> selected;
  std::shared_ptr<const entry> reusable;
  result ready;
  const auto priority = options.priority.value_or(identity.declared_priority);
  try {
    validate_key(identity);
    if (!rebuild || options.max_value_bytes == 0) {
      throw std::invalid_argument{"Invalid cache rebuild request"};
    }
    if (options.check_cancelled) {
      options.check_cancelled();
    }
    {
      std::unique_lock guard{state_->mutex};
      auto found = state_->slots.find(identity);
      if (found == state_->slots.end()) {
        if (state_->slots.size() >= state_->max_entries) {
          auto candidate = state_->slots.end();
          for (auto item = state_->slots.begin(); item != state_->slots.end(); ++item) {
            if (item->second->state == status::building || item->second.use_count() != 1 ||
                (item->second->completed.pin && item->second->completed.pin.use_count() != 1)) {
              continue;
            }
            if (candidate == state_->slots.end() ||
                std::tuple{item->second->completed.pin ? item->second->completed.pin->priority : 0,
                           item->second->last_access, item->first} <
                    std::tuple{candidate->second->completed.pin
                                   ? candidate->second->completed.pin->priority
                                   : 0,
                               candidate->second->last_access, candidate->first}) {
              candidate = item;
            }
          }
          if (candidate == state_->slots.end()) {
            throw std::length_error{"Cache service slot budget exceeded"};
          }
          state_->slots.erase(candidate);
        }
        found = state_->slots.emplace(identity, std::make_shared<implementation::slot>()).first;
      }
      selected = found->second;
      if (state_->clock == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error{"Cache access clock exhausted"};
      }
      selected->last_access = ++state_->clock;
      while (selected->state == status::building) {
        if (selected->builder == std::this_thread::get_id()) {
          throw std::logic_error{"Recursive rebuild of the same cache key"};
        }
        selected->changed.wait_for(guard, std::chrono::milliseconds{25});
        guard.unlock();
        if (options.check_cancelled) {
          options.check_cancelled();
        }
        guard.lock();
      }
      if (selected->state == status::ready && !selected->invalidated &&
          selected->completed.pin->priority == priority) {
        ready = selected->completed;
      } else {
        if (selected->state == status::ready && !selected->invalidated) {
          reusable = selected->completed.pin;
        }
        selected->state = status::building;
        selected->builder = std::this_thread::get_id();
        selected->invalidated = false;
      }
    }
    const auto check = [&](std::span<const std::uint8_t> value) {
      if (options.check_cancelled) {
        options.check_cancelled();
      }
      if (value.size() > options.max_value_bytes) {
        throw std::length_error{"Cache rebuild result budget exceeded"};
      }
      if (options.validate && !options.validate(value)) {
        throw std::invalid_argument{"Cache value validation failed"};
      }
      if (options.source_is_current && !options.source_is_current()) {
        throw std::runtime_error{"Cache source changed before publication"};
      }
    };
    if (ready.pin) {
      check(ready.pin->value);
      return ready;
    }
    result completed;
    completed.pin = reusable;
    if (!completed.pin && state_->storage) {
      try {
        completed.pin = state_->storage->lookup(identity);
      } catch (...) {
        completed.storage_error = std::current_exception();
      }
    }
    if (completed.pin) {
      try {
        check(completed.pin->value);
      } catch (...) {
        completed.pin.reset();
        if (state_->storage) {
          try {
            state_->storage->invalidate(identity);
          } catch (...) {
            completed.storage_error = std::current_exception();
          }
        }
      }
    }
    if (!completed.pin) {
      if (options.check_cancelled) {
        options.check_cancelled();
      }
      if (options.source_is_current && !options.source_is_current()) {
        throw std::runtime_error{"Cache source is no longer current"};
      }
      auto value = rebuild();
      check(value);
      if (state_->max_resident_bytes < sizeof(entry) ||
          value.capacity() > state_->max_resident_bytes - sizeof(entry)) {
        throw std::length_error{"Cache retained allocation budget exceeded"};
      }
      completed.pin = std::make_shared<const entry>(entry{identity, priority, std::move(value)});
      if (state_->storage) {
        try {
          state_->storage->publish(identity, completed.pin->value, priority);
        } catch (...) {
          completed.storage_error = std::current_exception();
        }
      }
    }
    if (completed.pin->priority != priority) {
      completed.pin =
          std::make_shared<const entry>(entry{identity, priority, completed.pin->value});
      if (state_->storage) {
        try {
          state_->storage->publish(identity, completed.pin->value, priority);
        } catch (...) {
          completed.storage_error = std::current_exception();
        }
      }
    }
    check(completed.pin->value);
    completed.state = status::ready;
    {
      std::lock_guard guard{state_->mutex};
      if (selected->invalidated) {
        throw std::runtime_error{"Cache invalidated during rebuild"};
      }
      const auto charge = sizeof(entry) + completed.pin->value.capacity();
      if (charge > state_->max_resident_bytes ||
          state_->retained_bytes() > state_->max_resident_bytes - charge) {
        throw std::length_error{"Cache resident byte budget exceeded"};
      }
      state_->residents.push_back(completed.pin);
      selected->completed = completed;
      selected->state = status::ready;
      selected->builder = {};
    }
    selected->changed.notify_all();
    return completed;
  } catch (...) {
    result failed{status::failed, {}, std::current_exception(), {}};
    if (selected && !ready.pin) {
      bool remove_stale{};
      {
        std::lock_guard guard{state_->mutex};
        remove_stale = selected->builder == std::this_thread::get_id() && selected->invalidated;
      }
      // Keep the slot building until cleanup completes, so a retry cannot race stale disk removal.
      if (remove_stale && state_->storage) {
        try {
          state_->storage->invalidate(identity);
        } catch (...) {
          failed.storage_error = std::current_exception();
        }
      }
      {
        std::lock_guard guard{state_->mutex};
        // A canceled waiter must not complete another thread's build.
        if (selected->builder == std::this_thread::get_id()) {
          selected->completed = failed;
          selected->state = status::failed;
          selected->builder = {};
        }
      }
      selected->changed.notify_all();
    }
    return failed;
  }
}

// Read readiness separately from payload defaults.
status service::state(const key& identity) const {
  std::lock_guard guard{state_->mutex};
  const auto found = state_->slots.find(identity);
  return found == state_->slots.end() || found->second->invalidated ? status::absent
                                                                    : found->second->state;
}

// Fence in-flight publication before invoking provider code outside the service mutex.
void service::invalidate(const key& identity) {
  {
    std::lock_guard guard{state_->mutex};
    const auto found = state_->slots.find(identity);
    if (found != state_->slots.end()) {
      found->second->invalidated = true;
      found->second->completed = {};
      if (found->second->state != status::building) {
        found->second->state = status::absent;
      }
    }
  }
  if (state_->storage) {
    state_->storage->invalidate(identity);
  }
}

} // namespace rohit::cache
