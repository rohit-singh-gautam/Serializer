// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <model.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream_io.hpp>
#if !defined(SERIALIZER_XML_BENCHMARK_NATIVE_ONLY)
#include <libxml/parser.h>
#include <libxml/xmlversion.h>
#include <rohit/xml.hpp>
#endif

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <psapi.h>
#elif !defined(__linux__)
#error This measurement driver currently supports Windows and Linux.
#endif

namespace {
namespace codec = rohit::serializer;
#if !defined(SERIALIZER_XML_BENCHMARK_NATIVE_ONLY)
namespace xml = codec::xml;
#endif
using model = xml_example::order;
using bytes = std::vector<std::uint8_t>;
constexpr std::size_t measured_iterations = 5;
constexpr std::size_t maximum_input_bytes = 8 * 1024 * 1024;
volatile std::size_t observed_result{};

// Read actual process CPU time; MSVC's clock() reports elapsed wall time instead.
std::uint64_t process_cpu_ns() {
#if defined(_WIN32)
  FILETIME created{}, exited{}, kernel{}, user{};
  if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
    throw std::system_error{static_cast<int>(GetLastError()), std::system_category(),
                            "Cannot read process CPU counters"};
  }
  // Join the system API's high/low words without depending on object layout.
  const auto ticks = [](const FILETIME& value) {
    return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
  };
  constexpr std::uint64_t nanoseconds_per_tick = 100;
  constexpr auto maximum_ticks = std::numeric_limits<std::uint64_t>::max() / nanoseconds_per_tick;
  const auto kernel_ticks = ticks(kernel), user_ticks = ticks(user);
  if (user_ticks > maximum_ticks || kernel_ticks > maximum_ticks - user_ticks) {
    throw std::overflow_error{"Process CPU counter exceeds measurement range"};
  }
  return (kernel_ticks + user_ticks) * nanoseconds_per_tick;
#else
  const auto ticks = std::clock();
  if (ticks == std::clock_t{-1}) {
    throw std::runtime_error{"Process CPU time is unavailable"};
  }
  const auto nanoseconds = static_cast<long double>(ticks) * (1e9L / CLOCKS_PER_SEC);
  if (nanoseconds > std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"Process CPU counter exceeds measurement range"};
  }
  return static_cast<std::uint64_t>(nanoseconds);
#endif
}

// Return the operating system's process high-water residency, including native libraries and setup.
std::size_t peak_resident_bytes() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS counters{};
  counters.cb = sizeof(counters);
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
    throw std::system_error{static_cast<int>(GetLastError()), std::system_category(),
                            "Cannot read process memory counters"};
  }
  return counters.PeakWorkingSetSize;
#else
  // getrusage can retain the parent's pre-exec peak. VmHWM belongs to this process image.
  std::ifstream status{"/proc/self/status"};
  std::string line{};
  while (std::getline(status, line)) {
    if (line.starts_with("VmHWM:")) {
      std::istringstream counter{line};
      std::string name{}, unit{};
      std::size_t kibibytes{};
      constexpr std::size_t bytes_per_kibibyte = 1024;
      if (!(counter >> name >> kibibytes >> unit) || unit != "kB" ||
          kibibytes > std::numeric_limits<std::size_t>::max() / bytes_per_kibibyte) {
        throw std::runtime_error{"Invalid process residency counter"};
      }
      return kibibytes * bytes_per_kibibyte;
    }
  }
  throw std::runtime_error{"Cannot read process residency"};
#endif
}

// Own one explicitly bounded fixture, without including file I/O in codec timings.
std::string read_input(const char* path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    throw std::runtime_error{"Cannot open benchmark input"};
  }
  const auto data = rohit::read_stream_bytes(input, maximum_input_bytes);
  return {reinterpret_cast<const char*>(data.begin()), data.current_offset()};
}

// Publish complete encoded bytes only after the measured operation and its value checks succeed.
void write_output(const char* path, const auto& encoded) {
  std::ofstream output{path, std::ios::binary | std::ios::trunc};
  output.exceptions(std::ios::badbit | std::ios::failbit);
  output.write(reinterpret_cast<const char*>(encoded.data()),
               static_cast<std::streamsize>(encoded.size()));
  output.close();
}

// Keep the ordinary generated binary/JSON path as an owning native-codec baseline.
template <template <codec::serialize_type> class Protocol>
bytes encode_native(const model& value) {
  rohit::full_stream_auto_alloc output;
  codec::serialize_to<Protocol>(output, value);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Require exact native-message completion when measuring fresh owning decoding.
template <template <codec::serialize_type> class Protocol>
model decode_native(const bytes& encoded) {
  const auto input = rohit::make_constant_full_stream(encoded.data(), encoded.size());
  return codec::deserialize_exact<model, Protocol>(input);
}

// Check all fields outside the timed interval instead of accepting a size-only round trip.
void require_equal(const model& expected, const model& actual) {
  if (expected.id != actual.id || expected.customer != actual.customer ||
      expected.count != actual.count || expected.tags != actual.tags) {
    throw std::runtime_error{"Benchmark owning values differ"};
  }
}

// Warm once and retain five codec wall/CPU samples; process residency is a separate OS high-water.
void measure(auto operation, auto observe, std::size_t encoded_bytes,
             std::size_t prepared_peak_bytes) {
  {
    const auto warm = operation();
    observed_result = observe(warm);
  }
  std::array<std::int64_t, measured_iterations> wall{};
  std::array<double, measured_iterations> cpu{};
  for (std::size_t index = 0; index < measured_iterations; ++index) {
    const auto started = std::chrono::steady_clock::now();
    const auto cpu_started = process_cpu_ns();
    const auto result = operation();
    const auto cpu_finished = process_cpu_ns();
    const auto finished = std::chrono::steady_clock::now();
    wall[index] = std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count();
    cpu[index] = static_cast<double>(cpu_finished - cpu_started);
    observed_result = observe(result);
  }
  std::cout << std::setprecision(17) << "{\"encoded_bytes\":" << encoded_bytes
            << ",\"prepared_process_peak_bytes\":" << prepared_peak_bytes
            << ",\"process_peak_resident_bytes\":" << peak_resident_bytes()
            << ",\"provider_compiled_version\":\""
#if defined(SERIALIZER_XML_BENCHMARK_NATIVE_ONLY)
            << "disabled\",\"provider_runtime_version\":\"disabled"
#else
            << LIBXML_DOTTED_VERSION << "\",\"provider_runtime_version\":\"" << xmlParserVersion
#endif
            << "\",\"wall_ns\":[";
  for (std::size_t index = 0; index < wall.size(); ++index) {
    if (index != 0) {
      std::cout << ',';
    }
    std::cout << wall[index];
  }
  std::cout << "],\"cpu_ns\":[";
  for (std::size_t index = 0; index < cpu.size(); ++index) {
    if (index != 0) {
      std::cout << ',';
    }
    std::cout << cpu[index];
  }
  std::cout << "]}\n";
}

// Measure one protocol's owning encode or decode with the exact same generated model and values.
template <template <codec::serialize_type> class Protocol>
void native_measurement(const model& value, bool decoding, const char* output) {
  const auto encoded = encode_native<Protocol>(value);
  require_equal(value, decode_native<Protocol>(encoded));
  const auto prepared_peak = peak_resident_bytes();
  if (decoding) {
    measure([&] { return decode_native<Protocol>(encoded); },
            [](const model& result) { return result.tags.size() + result.customer.size(); },
            encoded.size(), prepared_peak);
  } else {
    measure([&] { return encode_native<Protocol>(value); },
            [](const bytes& result) { return result.size(); }, encoded.size(), prepared_peak);
  }
  write_output(output, encoded);
}

#if !defined(SERIALIZER_XML_BENCHMARK_NATIVE_ONLY)
// Interrupt complete owning operations before/mid traversal and on deadline, then verify fresh recovery.
void interruption_controls(const model& value, std::string_view document) {
  const auto canonical = encode_native<codec::binary_none>(value);
  constexpr std::array<std::size_t, 2> thresholds{1U, 1000U};
  std::size_t verified{};
  for (const bool writing : {false, true}) {
    for (const auto threshold : thresholds) {
      xml::operation_context context{};
      std::size_t polls{};
      context.is_cancelled = [&] { return ++polls >= threshold; };
      bool cancelled{};
      try {
        if (writing) { (void)xml::serialize(value, {}, nullptr, &context); }
        else { (void)xml::deserialize<model>(document, {}, nullptr, &context); }
      } catch (const std::runtime_error& error) {
        cancelled = std::string_view{error.what()} == "XML operation cancelled";
      }
      if (!cancelled || polls != threshold) {
        throw std::runtime_error{"XML cancellation returned a candidate or changed its failure"};
      }
      ++verified;
    }
    xml::operation_context expired{};
    expired.set_timeout(std::chrono::steady_clock::duration::zero());
    bool timed_out{};
    try {
      if (writing) { (void)xml::serialize(value, {}, nullptr, &expired); }
      else { (void)xml::deserialize<model>(document, {}, nullptr, &expired); }
    } catch (const std::runtime_error& error) {
      timed_out = std::string_view{error.what()} == "XML operation deadline exceeded";
    }
    if (!timed_out) { throw std::runtime_error{"Expired XML operation returned a candidate"}; }
    ++verified;
  }
  if (canonical != encode_native<codec::binary_none>(value)) {
    throw std::runtime_error{"Interrupted XML encoding changed its source"};
  }
  require_equal(value, xml::deserialize<model>(document));
  require_equal(value, xml::deserialize<model>(xml::serialize(value)));
  std::cout << "{\"verified_interruptions\":" << verified
            << ",\"mid_operation_poll\":" << thresholds.back()
            << ",\"fresh_decode_and_encode_recovered\":true}\n";
}
#endif
} // namespace

// Run one isolated operation per process so peak residency is not inherited from another profile.
int main(int argc, char** argv) {
  try {
    if (argc != 4) {
      throw std::invalid_argument{"Expected MODE INPUT.xml OUTPUT"};
    }
    const std::string_view mode{argv[1]};
    const auto document = read_input(argv[2]);
#if defined(SERIALIZER_XML_BENCHMARK_NATIVE_ONLY)
    const auto value = decode_native<codec::binary_none>(bytes{document.begin(), document.end()});
#else
    const auto value = xml::deserialize<model>(document);
    if (mode == "xml-interruptions") {
      interruption_controls(value, document);
      return 0;
    }
    if (mode == "xml-encode" || mode == "xml-decode") {
      const auto encoded = xml::serialize(value);
      require_equal(value, xml::deserialize<model>(encoded));
      const auto prepared_peak = peak_resident_bytes();
      if (mode == "xml-encode") {
        measure([&] { return xml::serialize(value); },
                [](const std::string& result) { return result.size(); }, encoded.size(),
                prepared_peak);
      } else {
        measure([&] { return xml::deserialize<model>(document); },
                [](const model& result) { return result.tags.size() + result.customer.size(); },
                document.size(), prepared_peak);
      }
      write_output(argv[3], encoded);
      return 0;
    }
#endif
    if (!mode.ends_with("-encode") && !mode.ends_with("-decode")) {
      throw std::invalid_argument{"Unknown benchmark operation"};
    }
    const bool decoding = mode.ends_with("-decode");
    if (mode.starts_with("JSON-")) {
      native_measurement<codec::json>(value, decoding, argv[3]);
    } else if (mode.starts_with("BINARY_NONE-")) {
      native_measurement<codec::binary_none>(value, decoding, argv[3]);
    } else if (mode.starts_with("BINARY_INTEGER-")) {
      native_measurement<codec::binary_integer>(value, decoding, argv[3]);
    } else if (mode.starts_with("BINARY_STRING-")) {
      native_measurement<codec::binary_string>(value, decoding, argv[3]);
    } else {
      throw std::invalid_argument{"Unknown benchmark mode"};
    }
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
