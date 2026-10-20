// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <ledger.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_journal_document.hpp>
#include <rohit/serializer.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {
namespace managed = rohit::managed;
namespace codec = rohit::serializer;
using ledger = ledger_example::ledger;
using store_type = managed::model_store<ledger>;
using bytes = std::vector<std::uint8_t>;
constexpr std::size_t max_document_bytes = 1024 * 1024;
constexpr std::size_t initial_stream_bytes = 4096;

// Keep the runnable lesson isolated from user documents and release only its own temporary files.
class temporary_document {
  std::filesystem::path directory_;

public:
  // Create a fresh uniquely named directory beneath the operating system's temporary directory.
  temporary_document() {
    const auto id = managed::make_document_id();
    directory_ =
        std::filesystem::temp_directory_path() /
        ("serializer-external-document-" + std::to_string(id.high) + "-" + std::to_string(id.low));
    if (!std::filesystem::create_directory(directory_)) {
      throw std::runtime_error{"Cannot create private example directory"};
    }
  }
  temporary_document(const temporary_document&) = delete;
  temporary_document& operator=(const temporary_document&) = delete;

  // Remove this example's verified private child after all stores and file handles have closed.
  ~temporary_document() {
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
  }

  // Return the ordinary JSON document path; native recovery companions share this private directory.
  std::filesystem::path path() const {
    return directory_ / "ledger.json";
  }
};

// Retain real checks in release builds rather than relying on disabled assertions.
void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error{message};
  }
}

// Bound both the external JSON decoder and the adapter's native storage conversion.
codec::decode_limits document_limits() {
  codec::decode_limits limits{};
  limits.max_input_bytes = max_document_bytes;
  limits.max_string_bytes = max_document_bytes / 2;
  limits.max_collection_elements = 1024;
  limits.max_nesting_depth = 16;
  return limits;
}

// Compose exact owning callbacks with native identity normalization and independent byte bounds.
auto document_codec() {
  return managed::make_journal_document_codec<ledger>(
      "example.ledger.json.v1",
      [](const ledger& value) -> bytes {
        const rohit::stream_limits bounds{initial_stream_bytes, max_document_bytes};
        rohit::full_stream_auto_alloc_limits output{&bounds};
        value.serialize_out<codec::json>(output);
        return {output.begin(), output.begin() + output.current_offset()};
      },
      [](std::span<const std::uint8_t> input) -> ledger {
        const auto stream = rohit::make_constant_full_stream(input.data(), input.size());
        return codec::deserialize_exact<ledger, codec::json>(stream, document_limits());
      },
      document_limits());
}
} // namespace

// Full Save/reopen keeps exact native IDs, retained redo and burned gaps alongside ordinary JSON.
int main() {
  try {
    const temporary_document fixture;
    const auto path = fixture.path();
    managed::journal_options options;
    options.document_codec = document_codec();
    bytes expected;
    std::uint32_t child_id{}, reserved_id{};
    {
      store_type store{ledger{"Base"}};
      store.create_journal(path, managed::journal_storage_mode::sidecar, options);
      store
          .execute_transaction([&](auto& tx) {
            tx.root().set_name("Edited");
            child_id = tx.root().entries().insert(10, {"Retained redo", 200});
          })
          .throw_if_failed();
      store
          .execute_transaction([&](auto& tx) {
            reserved_id = tx.create_id();
            tx.revert();
          })
          .throw_if_failed();
      store.undo();
      store.save_journal();
      expected = store.save();
    }
    {
      std::ifstream input{path, std::ios::binary};
      require(static_cast<bool>(input), "Cannot inspect saved JSON");
      const bytes data{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
      const auto stream = rohit::make_constant_full_stream(data.data(), data.size());
      const auto saved = codec::deserialize_exact<ledger, codec::json>(stream, document_limits());
      require(saved.name == "Base" && saved.entries.empty() && saved.persistent_id == 0,
              "Document must contain the normalized saved model");
    }
    store_type reopened{ledger{}};
    reopened.recover_journal(path, options);
    require(reopened.save() == expected, "Exact native state/history was not restored");
    reopened.redo();
    require(reopened.read()->name == "Edited" &&
                reopened.read()->entries.at(10).persistent_id == child_id,
            "Redo lost the original child identity");
    reopened.undo();
    reopened
        .execute_transaction([&](auto& tx) {
          require(tx.root().entries().insert(20, {"Fresh", 300}) > reserved_id,
                  "Reopen reused a burned native identity");
        })
        .throw_if_failed();
    std::cout << "JSON Save/reopen preserved native IDs, redo and burned allocation gaps.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
