// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include <rohit/file_stream.hpp>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <span>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace rohit {
namespace {
// Report the immediately preceding native file error without swallowing its platform code.
[[noreturn]] void io_error(const char* operation) {
#ifdef _WIN32
  throw std::system_error{static_cast<int>(GetLastError()), std::system_category(), operation};
#else
  throw std::system_error{errno, std::generic_category(), operation};
#endif
}

} // namespace

// Native handles own their lifetime; destructors never obscure an earlier I/O failure.
struct file_stream::implementation {
#ifdef _WIN32
  HANDLE handle_{INVALID_HANDLE_VALUE};
#else
  int handle_{-1};
#endif

public:
  // Open a bounded file or acquire the stable nonblocking single-writer lock.
  implementation(const std::filesystem::path& path, file_open_mode access) {
    if (access != file_open_mode::read && access != file_open_mode::update &&
        access != file_open_mode::create && access != file_open_mode::lock) {
      throw std::invalid_argument{"Invalid file stream open mode"};
    }
#ifdef _WIN32
    const auto disposition = access == file_open_mode::create
                                 ? CREATE_NEW
                                 : (access == file_open_mode::lock ? OPEN_ALWAYS : OPEN_EXISTING);
    const auto rights =
        access == file_open_mode::read ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE;
    const DWORD sharing = access == file_open_mode::lock ? 0 : FILE_SHARE_READ | FILE_SHARE_DELETE;
    handle_ = CreateFileW(path.c_str(), rights, sharing, nullptr, disposition,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
      io_error("Open file stream");
    }
#else
    const auto flags = access == file_open_mode::read
                           ? O_RDONLY
                           : (access == file_open_mode::create
                                  ? O_RDWR | O_CREAT | O_EXCL
                                  : (access == file_open_mode::lock ? O_RDWR | O_CREAT : O_RDWR));
    handle_ = ::open(path.c_str(), flags | O_CLOEXEC, S_IRUSR | S_IWUSR);
    if (handle_ < 0) {
      io_error("Open file stream");
    }
    if (access == file_open_mode::lock && ::flock(handle_, LOCK_EX | LOCK_NB) != 0) {
      const auto error = errno;
      ::close(handle_);
      errno = error;
      io_error("Lock file stream");
    }
#endif
  }
  implementation(const implementation&) = delete;
  implementation& operator=(const implementation&) = delete;

  // Closing the stable lock releases ownership even after abrupt process termination.
  ~implementation() {
#ifdef _WIN32
    CloseHandle(handle_);
#else
    ::close(handle_);
#endif
  }

  // Query length before allocating or accepting an append position.
  std::uint64_t size() const {
#ifdef _WIN32
    LARGE_INTEGER result{};
    if (!GetFileSizeEx(handle_, &result)) {
      io_error("Size file stream");
    }
    return static_cast<std::uint64_t>(result.QuadPart);
#else
    struct stat result{};
    if (::fstat(handle_, &result) != 0 || result.st_size < 0) {
      io_error("Size file stream");
    }
    return static_cast<std::uint64_t>(result.st_size);
#endif
  }

  // Move to a previously validated offset; native signed file offsets are checked explicitly.
  void seek(std::uint64_t offset) {
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      throw std::length_error{"File stream offset exceeds native range"};
    }
#ifdef _WIN32
    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(handle_, position, nullptr, FILE_BEGIN)) {
      io_error("Seek file stream");
    }
#else
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) ||
        ::lseek(handle_, static_cast<off_t>(offset), SEEK_SET) < 0) {
      io_error("Seek file stream");
    }
#endif
  }

  // Query the native cursor only when reconciling a borrowed lookahead byte.
  std::uint64_t position() const {
#ifdef _WIN32
    LARGE_INTEGER distance{};
    LARGE_INTEGER result{};
    if (!SetFilePointerEx(handle_, distance, &result, FILE_CURRENT)) {
      io_error("Query file stream position");
    }
    return static_cast<std::uint64_t>(result.QuadPart);
#else
    const auto result = ::lseek(handle_, 0, SEEK_CUR);
    if (result < 0) {
      io_error("Query file stream position");
    }
    return static_cast<std::uint64_t>(result);
#endif
  }

  // Complete short native reads, returning the delivered prefix on ordinary EOF.
  std::size_t read(std::span<std::uint8_t> output) {
    const auto requested_bytes = output.size();
    while (!output.empty()) {
#ifdef _WIN32
      const auto requested = static_cast<DWORD>(
          std::min<std::size_t>(output.size(), std::numeric_limits<DWORD>::max()));
      DWORD count{};
      if (!ReadFile(handle_, output.data(), requested, &count, nullptr)) {
        io_error("Read file stream");
      }
#else
      const auto requested = std::min<std::size_t>(
          output.size(), static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
      const auto count = ::read(handle_, output.data(), requested);
      if (count < 0) {
        if (errno == EINTR) {
          continue;
        }
        io_error("Read file stream");
      }
#endif
      if (count == 0) {
        break;
      }
      output = output.subspan(static_cast<std::size_t>(count));
    }
    return requested_bytes - output.size();
  }

  // Complete short writes; any error after this operation starts has an uncertain durable outcome.
  void write(std::span<const std::uint8_t> input) {
    while (!input.empty()) {
#ifdef _WIN32
      const auto requested = static_cast<DWORD>(
          std::min<std::size_t>(input.size(), std::numeric_limits<DWORD>::max()));
      DWORD count{};
      if (!WriteFile(handle_, input.data(), requested, &count, nullptr)) {
        io_error("Write file stream");
      }
#else
      const auto requested = std::min<std::size_t>(
          input.size(), static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
      const auto count = ::write(handle_, input.data(), requested);
      if (count < 0) {
        if (errno == EINTR) {
          continue;
        }
        io_error("Write file stream");
      }
#endif
      if (count == 0) {
        throw std::runtime_error{"Zero-length file stream write"};
      }
      input = input.subspan(static_cast<std::size_t>(count));
    }
  }

  // Flush data and file metadata before acknowledging a durable operation.
  void flush() {
#ifdef _WIN32
    if (!FlushFileBuffers(handle_)) {
      io_error("Flush file stream");
    }
#else
    while (::fsync(handle_) != 0) {
      if (errno != EINTR) {
        io_error("Flush file stream");
      }
    }
#endif
  }

  // Truncate only a validated incomplete tail, never a committed undo/redo suffix.
  void truncate(std::uint64_t offset) {
    seek(offset);
#ifdef _WIN32
    if (!SetEndOfFile(handle_)) {
      io_error("Repair file stream tail");
    }
#else
    if (::ftruncate(handle_, static_cast<off_t>(offset)) != 0) {
      io_error("Repair file stream tail");
    }
#endif
  }
};

// Construct the native stream state once; normal writes retain the positioned handle.
file_stream::file_stream(const std::filesystem::path& path, file_open_mode mode)
    : state_{std::make_unique<implementation>(path, mode)} {}

// The implementation owns and closes the platform handle.
file_stream::~file_stream() = default;

// Reconcile lookahead with a complete read, preserving standard EOF and gcount behavior.
void file_stream::read(char* bytes, std::streamsize size) {
  count_ = 0;
  if (size < 0 || fail()) {
    throw std::ios_base::failure{"File stream is not readable"};
  }
  try {
    if (size != 0 && lookahead_) {
      *bytes++ = static_cast<char>(*lookahead_);
      lookahead_.reset();
      ++count_;
    }
    count_ += static_cast<std::streamsize>(state_->read(
        {reinterpret_cast<std::uint8_t*>(bytes), static_cast<std::size_t>(size - count_)}));
    if (count_ != size) {
      eof_ = true;
      failed_ = true;
    }
  } catch (...) {
    failed_ = bad_ = true;
    throw;
  }
}

// Keep the no-lookahead append path free of seeks and extra payload buffers.
void file_stream::write(const char* bytes, std::streamsize size) {
  if (size < 0 || fail()) {
    throw std::ios_base::failure{"File stream is not writable"};
  }
  try {
    if (lookahead_) {
      state_->seek(state_->position() - 1);
      lookahead_.reset();
    }
    state_->write({reinterpret_cast<const std::uint8_t*>(bytes), static_cast<std::size_t>(size)});
  } catch (...) {
    failed_ = bad_ = true;
    throw;
  }
}

// Cache a single native byte so repeated peeks never change the logical position.
std::char_traits<char>::int_type file_stream::peek() {
  if (bad_ || (failed_ && !eof_)) {
    throw std::ios_base::failure{"File stream is not readable"};
  }
  if (eof_) {
    return std::char_traits<char>::eof();
  }
  try {
    if (!lookahead_) {
      std::uint8_t byte{};
      if (state_->read({&byte, 1}) == 0) {
        eof_ = true;
        return std::char_traits<char>::eof();
      }
      lookahead_ = byte;
    }
    return std::char_traits<char>::to_int_type(static_cast<char>(*lookahead_));
  } catch (...) {
    failed_ = bad_ = true;
    throw;
  }
}

// Query size independently of EOF and buffered lookahead.
std::uint64_t file_stream::size() const {
  return state_->size();
}

// A successful explicit seek starts a fresh read position; durable outcome remains a caller concern.
void file_stream::seek(std::uint64_t offset_bytes) {
  state_->seek(offset_bytes);
  lookahead_.reset();
  count_ = 0;
  eof_ = failed_ = bad_ = false;
}

// Discard only the caller-selected suffix and reset the stream at its new end.
void file_stream::truncate(std::uint64_t size_bytes) {
  state_->truncate(size_bytes);
  lookahead_.reset();
  count_ = 0;
  eof_ = failed_ = bad_ = false;
}

// A failed durable synchronization must never be reported as a successful byte write.
void file_stream::sync() {
  if (fail()) {
    throw std::ios_base::failure{"Cannot synchronize a failed file stream"};
  }
  try {
    state_->flush();
  } catch (...) {
    failed_ = bad_ = true;
    throw;
  }
}

// Persist directory entries as well as file contents on POSIX filesystems.
void sync_parent_directory(const std::filesystem::path& path) {
#ifdef _WIN32
  // Publication uses same-directory MoveFileExW with WRITE_THROUGH; no portable directory fsync.
  static_cast<void>(path);
#else
  const auto parent = path.parent_path().empty() ? std::filesystem::path{"."} : path.parent_path();
  const auto descriptor = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (descriptor < 0) {
    io_error("Open file stream directory");
  }
  int result{};
  do {
    result = ::fsync(descriptor);
  } while (result != 0 && errno == EINTR);
  const auto error = errno;
  ::close(descriptor);
  if (result != 0) {
    errno = error;
    io_error("Flush file stream directory");
  }
#endif
}

// Publish only a fully flushed same-directory replacement; no copy/delete fallback is permitted.
void publish_file(const std::filesystem::path& source, const std::filesystem::path& target,
                  bool replace) {
#ifdef _WIN32
  const auto flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
  if (!MoveFileExW(source.c_str(), target.c_str(), flags)) {
    io_error("Publish file stream generation");
  }
#else
  if (replace) {
    if (::rename(source.c_str(), target.c_str()) != 0) {
      io_error("Publish file stream generation");
    }
  } else {
    // link is an atomic create-if-absent; an existing document is never overwritten.
    if (::link(source.c_str(), target.c_str()) != 0) {
      io_error("Create file stream generation");
    }
    // The new name is authoritative; a leftover staging link is harmless.
    ::unlink(source.c_str());
  }
#endif
}

} // namespace rohit
