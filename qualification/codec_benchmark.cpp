#include <rohit/serializer.hpp>
#include <records.hpp>

#ifdef SERIALIZER_ALLOCATION_PROFILE
#include "allocation_metrics.hpp"
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace qualification {
// Implemented separately with LTO disabled so encoded buffers and decoded objects remain observable.
void observe(const void* data, std::size_t size) noexcept;
}

namespace {
#ifdef SERIALIZER_ALLOCATION_PROFILE
using output_stream = qualification::memory::profiled_stream;
#else
using output_stream = rohit::full_stream_auto_alloc;
#endif
constexpr std::size_t sample_count = 9;
constexpr std::size_t warmup_messages = 16;
constexpr std::size_t default_iterations = 100;

// Keep setup, verification, reporting, and warmup outside the timed region.
template <typename Operation>
void measure(std::string_view workload, std::string_view protocol, std::string_view operation,
             std::string_view storage, std::size_t encoded_bytes, std::size_t iterations,
             Operation&& perform) {
  for (std::size_t index = 0; index < warmup_messages; ++index) { perform(); }
  std::array<double, sample_count> samples{};
  std::uint64_t allocation_count{};
  std::uint64_t allocation_bytes{};
  std::uint64_t peak_bytes{};
  std::uint64_t peak_temporary_bytes{};
  for (auto& sample : samples) {
#ifdef SERIALIZER_ALLOCATION_PROFILE
    qualification::memory::start();
#endif
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < iterations; ++index) { perform(); }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    sample = std::chrono::duration<double, std::nano>(elapsed).count() / static_cast<double>(iterations);
#ifdef SERIALIZER_ALLOCATION_PROFILE
    const auto metrics = qualification::memory::finish();
    allocation_count += metrics.allocations;
    allocation_bytes += metrics.requested_bytes;
    peak_bytes = std::max(peak_bytes, metrics.peak_bytes);
    peak_temporary_bytes = std::max(peak_temporary_bytes, metrics.peak_bytes - metrics.live_bytes);
#endif
  }
  std::sort(samples.begin(), samples.end());
  constexpr double nanoseconds_per_second = 1e9;
  const auto median = samples[samples.size() / 2];
  const auto messages_per_second = median == 0 ? 0 : nanoseconds_per_second / median;
  const auto messages = static_cast<double>(iterations) * static_cast<double>(sample_count);
  std::cout << workload << ',' << protocol << ',' << operation << ',' << storage << ','
            << encoded_bytes << ',' << samples.front() << ',' << median << ',' << samples.back()
            << ',' << messages_per_second << ','
            << static_cast<double>(allocation_count) / messages << ','
            << static_cast<double>(allocation_bytes) / messages << ','
            << peak_bytes << ',' << peak_temporary_bytes << '\n';
}

// Verify round-trip bytes first, then measure encoding and decoding independently.
template <template <rohit::serializer::serialize_type> class Protocol, typename T>
void measure_protocol(std::string_view workload, std::string_view name, const T& value,
                      std::size_t iterations) {
  rohit::full_stream_auto_alloc reference{1};
  value.template serialize_out<Protocol>(reference);
  const auto encoded_bytes = reference.current_offset();
  const auto input = rohit::make_constant_full_stream(reference.begin(), encoded_bytes);
  T restored{};
  Protocol<rohit::serializer::serialize_type::in> decoder{input};
  decoder.serialize_in(restored);
  decoder.finish();
  rohit::full_stream_auto_alloc verified{1};
  restored.template serialize_out<Protocol>(verified);
  if (verified.current_offset() != encoded_bytes ||
      (encoded_bytes != 0 && std::memcmp(reference.begin(), verified.begin(), encoded_bytes) != 0)) {
    throw std::runtime_error{"Benchmark round-trip verification failed"};
  }

  for (const bool force_growth : {false, true}) {
    measure(workload, name, "encode", force_growth ? "fresh_growth" : "fresh_fit", encoded_bytes,
            iterations, [&] {
      output_stream output{force_growth ? 1 : encoded_bytes};
      value.template serialize_out<Protocol>(output);
      qualification::observe(output.begin(), output.current_offset());
    });
  }
  output_stream reused_output{encoded_bytes};
  measure(workload, name, "encode", "reused_fit", encoded_bytes, iterations, [&] {
    reused_output.reset();
    value.template serialize_out<Protocol>(reused_output);
    qualification::observe(reused_output.begin(), reused_output.current_offset());
  });
  measure(workload, name, "decode", "fresh", encoded_bytes, iterations, [&] {
    const auto bytes = rohit::make_constant_full_stream(reference.begin(), encoded_bytes);
    Protocol<rohit::serializer::serialize_type::in> reader{bytes};
    T destination{};
    reader.serialize_in(destination);
    reader.finish();
    qualification::observe(&destination, sizeof(destination));
  });
  T reused_destination{};
  measure(workload, name, "decode", "reused", encoded_bytes, iterations, [&] {
    const auto bytes = rohit::make_constant_full_stream(reference.begin(), encoded_bytes);
    Protocol<rohit::serializer::serialize_type::in> reader{bytes};
    reader.serialize_in(reused_destination);
    reader.finish();
    qualification::observe(&reused_destination, sizeof(reused_destination));
  });
}

// Run identical values through each supported codec mode.
template <typename T>
void measure_workload(std::string_view name, const T& value, std::size_t iterations) {
  measure_protocol<rohit::serializer::json>(name, "json", value, iterations);
  measure_protocol<rohit::serializer::binary_none>(name, "positional", value, iterations);
  measure_protocol<rohit::serializer::binary_integer>(name, "integer_key", value, iterations);
  measure_protocol<rohit::serializer::binary_string>(name, "string_key", value, iterations);
}
} // namespace

// Emit CSV samples; callers choose an optimized build and record the host/architecture separately.
int main(int argc, char** argv) {
  try {
    std::size_t iterations = default_iterations;
    if (argc > 2) { throw std::invalid_argument{"Usage: codec_benchmark [iterations_per_sample]"}; }
    if (argc == 2) {
      const std::string_view argument{argv[1]};
      const auto parsed = std::from_chars(argument.data(), argument.data() + argument.size(), iterations);
      constexpr std::size_t maximum_iterations = 1000000;
      if (parsed.ec != std::errc{} || parsed.ptr != argument.data() + argument.size() ||
          iterations == 0 || iterations > maximum_iterations) {
        throw std::invalid_argument{"Iteration count must be in 1..1000000"};
      }
    }
    std::cout << "# compiler=" << SERIALIZER_BENCHMARK_COMPILER
              << ",configuration=" << SERIALIZER_BENCHMARK_CONFIGURATION
              << ",pointer_bits=" << sizeof(void*) * 8 << ",iterations=" << iterations
              << ",samples=" << sample_count << '\n';
#ifdef SERIALIZER_ALLOCATION_PROFILE
    std::cout << "# allocation profile: timing includes instrumentation; peaks count requested payload bytes\n";
#else
    std::cout << "# timing only: allocation columns are zero; run codec_allocation_profile separately\n";
#endif
    std::cout << "workload,protocol,operation,storage,encoded_bytes,min_ns,median_ns,max_ns,messages_per_second,"
                 "allocations_per_message,requested_bytes_per_message,peak_new_bytes,peak_temporary_bytes\n";
    qualification::scalars scalars{};
    scalars.sequence = 123456789;
    scalars.signed_value = -123;
    scalars.single_value = 0.25f;
    scalars.double_value = 0.1;
    scalars.enabled = true;
    measure_workload("scalars", scalars, iterations);
    qualification::record value{};
    value.header = scalars;
    value.payload.first.value = 7;
    value.text = "short text";
    measure_workload("short_text", value, iterations);
    constexpr std::size_t long_text_bytes = 16 * 1024;
    value.text.assign(long_text_bytes, 'x');
    measure_workload("long_text", value, iterations);
    value.text = "escaped \"text\" with a newline\n";
    constexpr std::size_t collection_size = 256;
    for (std::size_t index = 0; index < collection_size; ++index) {
      value.integers.push_back(index * 1234567);
      value.floats.push_back(static_cast<double>(index) / 10);
      value.objects.push_back(scalars);
      value.lookup.emplace(static_cast<std::uint32_t>(index), scalars);
    }
    measure_workload("nested_collections", value, iterations);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
