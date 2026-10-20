// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/file_stream.hpp>
#include <rohit/package.hpp>
#include <rohit/xml.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
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
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// GNU ld routes only the test executable's native file calls through these shims.
extern "C" int __real_open(const char*, int, ...);
extern "C" ssize_t __real_write(int, const void*, std::size_t);
extern "C" int __real_fsync(int);
extern "C" int __real_rename(const char*, const char*);
extern "C" int __real_close(int);
#endif

namespace package_save_fault {
enum class point { none, create, partial_write, file_sync, publish, directory_sync };

// Scope faults to the exclusively owned staging file and its publication operation.
struct state {
  point selected{};
  bool reached{};
  std::size_t live_files{};
#ifdef _WIN32
  HANDLE staging{INVALID_HANDLE_VALUE};
#else
  int staging{-1};
  int directory{-1};
#endif
};
state* active{};

// Match only Serializer's staging basename, never arbitrary application files.
bool staging_name(const std::filesystem::path& path) {
  return path.filename().string().starts_with(".serializer-save-");
}

#ifdef _WIN32
// Forward native exclusive creation, optionally denying this operation's staging file.
HANDLE WINAPI create_file(LPCWSTR path, DWORD access, DWORD sharing, LPSECURITY_ATTRIBUTES security,
                          DWORD disposition, DWORD flags, HANDLE template_file) {
  const bool staging = active && staging_name(path) && disposition == CREATE_NEW;
  if (staging && active->selected == point::create) {
    active->reached = true;
    SetLastError(ERROR_ACCESS_DENIED);
    return INVALID_HANDLE_VALUE;
  }
  const auto result =
      ::CreateFileW(path, access, sharing, security, disposition, flags, template_file);
  if (staging && result != INVALID_HANDLE_VALUE) {
    active->staging = result;
    ++active->live_files;
  }
  return result;
}

// Deliver a physical prefix before reporting a native write failure.
BOOL WINAPI write_file(HANDLE file, LPCVOID bytes, DWORD size, LPDWORD written,
                       LPOVERLAPPED overlapped) {
  if (active && file == active->staging && active->selected == point::partial_write) {
    active->reached = true;
    constexpr DWORD prefix_bytes = 5;
    const auto result = ::WriteFile(file, bytes, std::min(size, prefix_bytes), written, overlapped);
    if (!result) {
      return result;
    }
    SetLastError(ERROR_DISK_FULL);
    return FALSE;
  }
  return ::WriteFile(file, bytes, size, written, overlapped);
}

// Fail the explicit file durability boundary without affecting borrowed streams.
BOOL WINAPI flush_file(HANDLE file) {
  if (active && file == active->staging && active->selected == point::file_sync) {
    active->reached = true;
    SetLastError(ERROR_WRITE_FAULT);
    return FALSE;
  }
  return ::FlushFileBuffers(file);
}

// Deny replacement before the platform changes the destination name.
BOOL WINAPI move_file(LPCWSTR source, LPCWSTR target, DWORD flags) {
  if (active && staging_name(source) && active->selected == point::publish) {
    active->reached = true;
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
  }
  return ::MoveFileExW(source, target, flags);
}

// Track release of the owned staging handle even after a latched write failure.
BOOL WINAPI close_handle(HANDLE file) {
  if (active && file == active->staging) {
    --active->live_files;
    active->staging = INVALID_HANDLE_VALUE;
  }
  return ::CloseHandle(file);
}
#else
// Interpose only native file-adapter opens; preserve all original creation flags/modes.
int open_file(const char* path, int flags, ...) {
  mode_t mode{};
  if ((flags & O_CREAT) != 0) {
    va_list arguments;
    va_start(arguments, flags);
    mode = static_cast<mode_t>(va_arg(arguments, int));
    va_end(arguments);
  }
  const bool staging = active && staging_name(path) && (flags & O_EXCL) != 0;
  if (staging && active->selected == point::create) {
    active->reached = true;
    errno = EACCES;
    return -1;
  }
  const auto result = __real_open(path, flags, mode);
  if (active && result >= 0) {
    if (staging) {
      active->staging = result;
      ++active->live_files;
    } else if ((flags & O_DIRECTORY) != 0) {
      active->directory = result;
      ++active->live_files;
    }
  }
  return result;
}

// Write a real prefix to the staging file, then fail its next low-level write.
ssize_t write_file(int file, const void* bytes, std::size_t size) {
  if (active && file == active->staging && active->selected == point::partial_write) {
    if (active->reached) {
      errno = ENOSPC;
      return -1;
    }
    active->reached = true;
    constexpr std::size_t prefix_bytes = 5;
    return __real_write(file, bytes, std::min(size, prefix_bytes));
  }
  return __real_write(file, bytes, size);
}

// Distinguish prepublication file sync from postpublication directory sync.
int sync_file(int file) {
  if (active && ((file == active->staging && active->selected == point::file_sync) ||
                 (file == active->directory && active->selected == point::directory_sync))) {
    active->reached = true;
    errno = EIO;
    return -1;
  }
  return __real_fsync(file);
}

// Fail replacement without altering the prior destination or taking a copying fallback.
int rename_file(const char* source, const char* target) {
  if (active && staging_name(source) && active->selected == point::publish) {
    active->reached = true;
    errno = EACCES;
    return -1;
  }
  return __real_rename(source, target);
}

// Observe release of both staging and directory descriptors on every path.
int close_file(int file) {
  if (active && (file == active->staging || file == active->directory)) {
    --active->live_files;
    if (file == active->staging) {
      active->staging = -1;
    } else {
      active->directory = -1;
    }
  }
  return __real_close(file);
}
#endif
} // namespace package_save_fault

// Compile the unchanged production file adapter with private native-call substitution.
// The static runtime supplies the real package implementation; no public test hook is added.
#ifdef _WIN32
#define CreateFileW package_save_fault::create_file
#define WriteFile package_save_fault::write_file
#define FlushFileBuffers package_save_fault::flush_file
#define MoveFileExW package_save_fault::move_file
#define CloseHandle package_save_fault::close_handle
#endif
#ifdef _WIN32
#include "../src/file_stream.cpp"
#undef CreateFileW
#undef WriteFile
#undef FlushFileBuffers
#undef MoveFileExW
#undef CloseHandle
#else
// Forward the linker shims without substituting C++ member names or standard headers.
extern "C" int __wrap_open(const char* path, int flags, ...) {
  mode_t mode{};
  if ((flags & O_CREAT) != 0) {
    va_list arguments;
    va_start(arguments, flags);
    mode = static_cast<mode_t>(va_arg(arguments, int));
    va_end(arguments);
  }
  return package_save_fault::open_file(path, flags, mode);
}

// Forward complete/partial writes to the operation-local injection state.
extern "C" ssize_t __wrap_write(int file, const void* bytes, std::size_t size) {
  return package_save_fault::write_file(file, bytes, size);
}

// Forward explicit durability boundaries with file/directory identity intact.
extern "C" int __wrap_fsync(int file) {
  return package_save_fault::sync_file(file);
}

// Forward atomic rename without changing the source or destination spelling.
extern "C" int __wrap_rename(const char* source, const char* target) {
  return package_save_fault::rename_file(source, target);
}

// Forward handle release even on an exception-unwinding path.
extern "C" int __wrap_close(int file) {
  return package_save_fault::close_file(file);
}
#endif

namespace {
namespace package = rohit::serializer::package;
namespace fault = package_save_fault;
constexpr std::string_view old_contents = "previous complete document";
constexpr std::string_view payload = "candidate full archive payload";

// Keep injected failures and owned native handles confined to one sequential operation.
class fault_scope {
public:
  fault::state observed{};
  // Activate exactly one fault after fixture setup has finished.
  explicit fault_scope(fault::point selected) : observed{selected} {
    if (fault::active) {
      throw std::logic_error{"Nested package fault scope"};
    }
    fault::active = &observed;
  }
  // Stop interposition before fixture destruction and subsequent tests.
  ~fault_scope() {
    fault::active = nullptr;
  }
};

// Own flat fixture files below the OS temporary root, with no external cleanup targets.
class package_save_fault_test : public testing::Test {
protected:
  std::filesystem::path directory_{};
  // Create a new directory for each independently injected native failure.
  void SetUp() override {
    directory_ = std::filesystem::temp_directory_path() /
                 ("serializer-package-save-fault-" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
  }
  // Remove only the flat files created under this test's known private directory.
  void TearDown() override {
    std::error_code ignored;
    for (const auto& entry : std::filesystem::directory_iterator{directory_}) {
      if (entry.is_regular_file()) {
        std::filesystem::remove(entry.path(), ignored);
      }
    }
    std::filesystem::remove(directory_, ignored);
  }
  // Save one ZIP or OPC candidate, with all production finish/sync/publish steps intact.
  void save(const std::filesystem::path& path, bool opc, package::limits configured = {}) {
    const std::span<const std::uint8_t> bytes{reinterpret_cast<const std::uint8_t*>(payload.data()),
                                              payload.size()};
    if (opc) {
      package::save_opc_file(
          path,
          [&](auto& writer) {
            writer.add_part("/model.bin", "application/octet-stream", bytes);
            writer.add_relationship({"/", "main", "urn:test:model", "model.bin"});
          },
          std::move(configured));
    } else {
      package::save_zip_file(
          path, [&](auto& writer) { writer.add("model.bin", bytes); }, std::move(configured));
    }
  }
  // Check complete metadata, CRC and exact payload before accepting a published archive.
  void expect_complete(const std::filesystem::path& path, bool opc) {
    std::ifstream input{path, std::ios::binary};
    package::zip_reader reader{input};
    reader.validate_all();
    const auto decoded = reader.read("model.bin");
    EXPECT_EQ(std::string(decoded.begin(), decoded.end()), payload);
    EXPECT_EQ(reader.entries().size(), opc ? 3U : 1U);
  }
  // Inspect the old destination byte-for-byte, without treating it as an archive.
  void expect_old(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>{input}, {}), old_contents);
  }
  // Ensure cleanup removed every staging name and released all observed native handles.
  void expect_clean(const fault_scope& scope) {
    EXPECT_EQ(scope.observed.live_files, 0U);
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator{directory_},
                            std::filesystem::directory_iterator{}),
              1);
  }
};

// Each real native prepublication failure retains the old file and removes partial staging.
TEST_F(package_save_fault_test, prepublication_native_failures_keep_old_file) {
  constexpr std::array failures{fault::point::create, fault::point::partial_write,
                                fault::point::file_sync, fault::point::publish};
  for (const bool opc : {false, true}) {
    if (opc && !rohit::serializer::xml::available()) {
      continue;
    }
    for (const auto selected : failures) {
      SCOPED_TRACE(opc);
      SCOPED_TRACE(static_cast<int>(selected));
      const auto destination = directory_ / "document.zip";
      {
        std::ofstream original{destination, std::ios::binary};
        original << old_contents;
      }
      fault_scope scope{selected};
      try {
        save(destination, opc);
        FAIL() << "Injected native failure was ignored";
      } catch (const package::error& failure) {
        EXPECT_EQ(failure.code, selected == fault::point::publish ? package::error_code::publication
                                                                  : package::error_code::io);
      } catch (const std::system_error&) {
        EXPECT_TRUE(selected == fault::point::partial_write || selected == fault::point::file_sync);
      }
      EXPECT_TRUE(scope.observed.reached);
      expect_old(destination);
      expect_clean(scope);
    }
  }
}

// Successful replacement yields a complete ZIP/OPC, including when the prior file is absent.
TEST_F(package_save_fault_test, successful_replacement_and_creation_publish_complete_files) {
  for (const bool opc : {false, true}) {
    if (opc && !rohit::serializer::xml::available()) {
      continue;
    }
    const auto destination = directory_ / "document.zip";
    std::error_code ignored;
    std::filesystem::remove(destination, ignored);
    fault_scope scope{fault::point::none};
    save(destination, opc);
    expect_complete(destination, opc);
    save(destination, opc);
    expect_complete(destination, opc);
    expect_clean(scope);
  }
}

#ifndef _WIN32
// A failed directory durability step reports uncertainty while retaining the complete new file.
TEST_F(package_save_fault_test, directory_sync_failure_retains_complete_new_file) {
  for (const bool opc : {false, true}) {
    if (opc && !rohit::serializer::xml::available()) {
      continue;
    }
    const auto destination = directory_ / "document.zip";
    {
      std::ofstream original{destination, std::ios::binary};
      original << old_contents;
    }
    fault_scope scope{fault::point::directory_sync};
    try {
      save(destination, opc);
      FAIL() << "Injected directory sync failure was ignored";
    } catch (const package::error& failure) {
      EXPECT_EQ(failure.code, package::error_code::publication_uncertain);
    }
    EXPECT_TRUE(scope.observed.reached);
    expect_complete(destination, opc);
    expect_clean(scope);
  }
}
#endif
} // namespace
