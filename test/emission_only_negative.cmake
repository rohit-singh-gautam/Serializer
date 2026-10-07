# Compile isolated failures after a valid control proves the compiler/include setup works.
if(NOT DEFINED COMPILER OR NOT DEFINED SOURCE_DIRECTORY OR NOT DEFINED GENERATED_DIRECTORY OR NOT DEFINED WORK_DIRECTORY)
  message(FATAL_ERROR "Constant binary compile checks require compiler and source/generated/work directories")
endif()
file(MAKE_DIRECTORY "${WORK_DIRECTORY}")
set(prefix [=[
#include "emission_only.hpp"
namespace codec = rohit::serializer;
namespace models = emission_models::emission;

]=])

# Compile one source and distinguish a deliberate constexpr rejection from toolchain failure.
function(compile_case name source expected_failure)
  set(input "${WORK_DIRECTORY}/${name}.cpp")
  set(object "${WORK_DIRECTORY}/${name}.obj")
  file(WRITE "${input}" "${prefix}${source}")
  if(MSVC_LIKE)
    set(arguments /nologo /std:c++20 /EHsc /c "/I${SOURCE_DIRECTORY}/include" "/I${GENERATED_DIRECTORY}" "${input}" "/Fo${object}")
  else()
    set(arguments -std=c++20 -c "-I${SOURCE_DIRECTORY}/include" "-I${GENERATED_DIRECTORY}" "${input}" -o "${object}")
  endif()
  execute_process(COMMAND "${COMPILER}" ${arguments} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(expected_failure)
    if(result EQUAL 0)
      message(FATAL_ERROR "Expected constant-evaluation rejection for ${name}")
    endif()
    if(NOT "${output}${errors}" MATCHES "constexpr|consteval|constant expression|C2131|C7595")
      message(FATAL_ERROR "Failure for ${name} was not a constant-evaluation diagnostic: ${output}${errors}")
    endif()
  elseif(NOT result EQUAL 0)
    message(FATAL_ERROR "Valid control failed: ${output}${errors}")
  endif()
endfunction()

compile_case(control [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { models::payload value{}; value.sequence = 42; value.label = "ok"; return value; }>(); static_assert(bytes.size() == 8 && bytes[0] == 42);]=] FALSE)
compile_case(fixed_extent [=[constexpr std::array<std::uint16_t, 2> values{1, 2}; constexpr auto bytes = codec::make_binary_none_bytes<[] { models::fixed_packet value{}; value.values = values; return value; }>();]=] TRUE)
compile_case(dangling_text [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { char text[]{'a'}; models::payload value{}; value.label = {text, 1}; return value; }>();]=] TRUE)
compile_case(dangling_span [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { std::uint8_t data[]{1}; models::payload value{}; value.data = data; return value; }>();]=] TRUE)
compile_case(invalid_text [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { models::payload value{}; value.label = std::string_view{"\x80", 1}; return value; }>();]=] TRUE)
compile_case(invalid_enum [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { models::enum_packet value{}; value.state = static_cast<models::phase>(2); return value; }>();]=] TRUE)
compile_case(invalid_union [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { models::choice value{}; value.data_type = static_cast<models::choice::e_data>(3); return value; }>();]=] TRUE)
compile_case(invalid_revision [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { models::special_packet value{}; value.version = 3; return value; }>();]=] TRUE)
compile_case(compact_overflow [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { models::special_packet value{}; value.prefix = 0x40000000; return value; }>();]=] TRUE)
compile_case(short_destination [=[constexpr auto bytes = [] { std::array<std::uint8_t, 1> memory{}; models::scalar_node value{42, true}; static_cast<void>(codec::serialize_binary_none_to(memory, value)); return memory; }();]=] TRUE)
message(STATUS "Verified emission-only control and nine constant-evaluation failures")
