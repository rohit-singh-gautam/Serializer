// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "identity.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace pkg = rohit::serializer::package;
using archive = identity_package::archive;
namespace {
constexpr std::uint64_t mebibyte = 1024U * 1024U;
constexpr std::string_view sentinel = "previous complete application file";
std::size_t passed{};

// Identify an original standard caller exception even when the platform copies exception storage.
class caller_failure final : public std::exception {
public:
  std::shared_ptr<const int> identity;
  // Retain the caller-owned token across normal exception copying and propagation.
  explicit caller_failure(std::shared_ptr<const int> token) : identity{std::move(token)} {}
  // Supply a stable diagnostic without allocating during exception handling.
  const char* what() const noexcept override { return "original large caller marker"; }
};

// Keep all assertions active in optimized actual SDK builds.
void require(bool value, const char* message) {
  if (!value) { throw std::runtime_error{message}; }
  ++passed;
}

// Bound every public operation while admitting the independent large corpus.
pkg::limits large_limits() {
  pkg::limits value;
  value.max_part_bytes = 16U * mebibyte;
  value.max_archive_bytes = 128U * mebibyte;
  value.max_io_bytes = 1024U * mebibyte;
  value.max_total_expanded_bytes = 512U * mebibyte;
  value.max_buffer_bytes = 256U * mebibyte;
  value.max_metadata_bytes = 64U * mebibyte;
  value.max_decoder_allocation_bytes = 2048U * mebibyte;
  value.max_work_units = 8192U * mebibyte;
  value.max_references = 4000000U;
  return value;
}

// Read only test-owned fixture/output files without interpreting OPC target text.
std::string read_file(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  require(input.good(), "open test file");
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

// Install a recognizable old application file before a transactional failure.
void old_file(const std::filesystem::path& path) {
  std::ofstream output{path, std::ios::binary | std::ios::trunc};
  output.write(sentinel.data(), static_cast<std::streamsize>(sentinel.size()));
  require(output.good(), "write sentinel");
}

// Choose the explicit caller-owned policy needed by inert vendor parts and links.
pkg::extension_policy preserve() {
  pkg::extension_policy policy;
  policy.parts = pkg::unknown_policy::preserve;
  policy.relationships = pkg::unknown_policy::preserve;
  return policy;
}

// Keep generated owning discovery/type/reference checks on the normal public load path.
auto load(const std::filesystem::path& path, pkg::limits limits,
    const std::function<void(const archive::value&, pkg::budget&)>& validate = {}) {
  std::ifstream input{path, std::ios::binary};
  require(input.good(), "open archive");
  return archive::load(input, std::move(limits), preserve(), validate);
}

// Map one exact public resource to its policy maximum without touching runtime internals.
void set_limit(pkg::limits& limits, pkg::resource resource, std::uint64_t amount) {
  switch (resource) {
    case pkg::resource::references: limits.max_references = amount; break;
    case pkg::resource::work_units: limits.max_work_units = amount; break;
    case pkg::resource::decoder_allocation_bytes: limits.max_decoder_allocation_bytes = amount; break;
    default: throw std::runtime_error{"unexpected measured resource"};
  }
}

// Inspect every large model through generated accessors rather than a handwritten XML decoder.
void inspect(const archive::value& value, std::size_t count) {
  require(value.registries.size() == 3U, "three independently scoped registries");
  for (const auto& item : value.registries) {
    require(item.value.requests.size() == count &&
        item.value.entries.size() == count, "complete large identity model");
  }
}
} // namespace

// Qualify large generated owning APIs, exact encode budgets, cancellation and atomic Save recovery.
int main(int argc, char** argv) {
  try {
    require(argc == 4, "fixture arguments");
    const std::filesystem::path directory{argv[1]};
    const auto count = static_cast<std::size_t>(std::stoull(argv[2]));
    const auto expected_references = std::stoull(argv[3]);
    const auto destination = directory / "produced.opc";
    std::uint64_t native_references{};
    auto loaded = load(directory / "valid.opc", large_limits(), [&](const auto& value, auto& budget) {
      inspect(value, count);
      native_references = budget.configured().max_references - budget.remaining(pkg::resource::references);
    });
    require(native_references == expected_references, "shared identity tuples and OPC relationships");
    require(loaded.extensions.parts.size() == 1U && loaded.extensions.relationships.size() == 3U,
        "preserved opaque owner and internal/external scopes");
    std::vector<pkg::model_part_view<identity_xml::registry>> views;
    for (const auto& item : loaded.value.registries) { views.push_back({&item.value, item.name, item.content_type, 0U}); }
    pkg::profile_overrides profiles;
    profiles["registries"].compression = rohit::serializer::compression::format::deflate;
    const archive::input value{views, &loaded.extensions, &profiles};
    const std::array resources{pkg::resource::references, pkg::resource::work_units, pkg::resource::decoder_allocation_bytes};
    std::array<std::uint64_t, resources.size()> used{};
    auto ledger = std::make_shared<pkg::budget>(large_limits());
    std::ostringstream encoded{std::ios::binary};
    archive::write(encoded, value, ledger);
    for (std::size_t index{}; index < resources.size(); ++index) {
      const auto resource = resources[index];
      const auto maximum = resource == pkg::resource::references ? ledger->configured().max_references :
          resource == pkg::resource::work_units ? ledger->configured().max_work_units : ledger->configured().max_decoder_allocation_bytes;
      used[index] = maximum-ledger->remaining(resource);
      require(used[index] > 0U, "measured cumulative encode resource");
      for (const bool tight : {false, true}) {
        auto limits = large_limits(); set_limit(limits, resource, used[index]-static_cast<unsigned>(tight));
        auto bounded = std::make_shared<pkg::budget>(limits);
        std::ostringstream output{std::ios::binary};
        bool succeeded{};
        try { archive::write(output, value, bounded); succeeded = true; }
        catch (const pkg::error& error) { require(tight && error.code == pkg::error_code::resource_limit, "one-tight encode resource failure"); }
        require(succeeded != tight, "exact encode admission versus one-tight rejection");
        old_file(destination);
        bool saved{};
        try { archive::save(destination, value, limits); saved = true; }
        catch (const pkg::error& error) { require(tight && error.code == pkg::error_code::resource_limit, "one-tight complete Save resource failure"); }
        require(saved != tight, "exact complete Save admission versus one-tight rejection");
        if (tight) { require(read_file(destination) == sentinel, "one-tight Save preserves previous complete file"); }
      }
    }
    bool validated{};
    try {
      static_cast<void>(load(directory / "invalid.opc", large_limits(), [&](const auto&, auto&) { validated = true; }));
      require(false, "invalid final-owner keyref accepted");
    } catch (const pkg::error& error) { require(error.code == pkg::error_code::codec && !validated, "late identity error before aggregate validation"); }

    std::size_t polls{};
    auto measured = large_limits(); measured.is_cancelled = [&] { ++polls; return false; };
    const auto started = std::chrono::steady_clock::now();
    archive::save(destination, value, measured);
    const auto elapsed = std::chrono::steady_clock::now()-started;
    require(polls > 3U, "large Save has observable cooperative boundaries");
    const std::array boundaries{std::size_t{1U}, std::max(std::size_t{2U}, polls/2U), polls};
    for (const bool clock : {false, true}) {
      for (const auto boundary : boundaries) {
        old_file(destination);
        std::size_t observed{};
        auto limits = large_limits();
        const auto marker = std::make_shared<const int>(7131);
        const auto deadline = std::chrono::steady_clock::now()+elapsed*2+std::chrono::milliseconds{30};
        limits.deadline = clock ? std::optional{deadline} : std::nullopt;
        limits.is_cancelled = [&] {
          if (++observed == boundary) {
            if (clock) { std::this_thread::sleep_until(deadline+std::chrono::milliseconds{2}); }
            else { throw caller_failure{marker}; }
          }
          return false;
        };
        bool failed{};
        try { archive::save(destination, value, limits); }
        catch (const pkg::error& error) { failed = true; require(clock && error.code == pkg::error_code::deadline_exceeded, "real host deadline category"); }
        catch (const caller_failure& error) { failed = true; require(!clock && error.identity == marker, "original standard callback exception identity"); }
        require(failed && read_file(destination) == sentinel, "interrupted large Save preserves old complete file");
      }
    }
    auto recovered = load(directory / "valid.opc", large_limits()); inspect(recovered.value, count);
    archive::save(destination, value, large_limits());
    auto reopened = load(destination, large_limits()); inspect(reopened.value, count);
    std::cout << "{\"checks\":" << passed << ",\"load_references\":" << native_references
              << ",\"encode_references\":" << used[0] << ",\"encode_work\":" << used[1]
              << ",\"encode_allocation\":" << used[2] << ",\"save_polls\":" << polls
              << ",\"interruptions\":6,\"exact_resource_scope\":\"public owning encode and atomic Save each retain one internal ledger; separate load/Save calls own separate ledgers\"}\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
