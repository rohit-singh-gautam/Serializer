# Compile isolated failures after a valid control proves the compiler/include setup works.
if(NOT DEFINED COMPILER OR NOT DEFINED SOURCE_DIRECTORY OR NOT DEFINED GENERATED_DIRECTORY OR NOT DEFINED WORK_DIRECTORY)
  message(FATAL_ERROR "Constant binary compile checks require compiler and source/generated/work directories")
endif()
file(MAKE_DIRECTORY "${WORK_DIRECTORY}")
set(prefix [=[
#include <rohit/constant_binary.hpp>
#include "constant_evaluation.hpp"
namespace codec = rohit::serializer;
namespace models = constant_binary_models;

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

compile_case(control [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { return std::uint32_t{42}; }>(); static_assert(bytes[0] == 42);]=] FALSE)
compile_case(invalid_text [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { return std::string_view{"\x80", 1}; }>();]=] TRUE)
compile_case(invalid_enum [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { return static_cast<models::phase>(2); }>();]=] TRUE)
compile_case(invalid_union [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { models::choice value{}; value.data_type = static_cast<models::choice::e_data>(3); return value; }>();]=] TRUE)
compile_case(compact_overflow [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { return codec::detail::compact_output<true, true, std::uint32_t>{0x40000000}; }>();]=] TRUE)
compile_case(invalid_revision [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { models::versioned value{}; value.version = 3; return value; }>();]=] TRUE)
compile_case(runtime_map [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { return models::runtime_map{}; }>();]=] TRUE)
compile_case(dangling_view [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { char text[]{'a'}; return std::string_view{text, 1}; }>();]=] TRUE)
compile_case(parameter_extent [=[consteval auto invalid(const auto& value) { constexpr auto size = codec::binary_none_size(value); return std::array<std::uint8_t, size>{}; } constexpr auto bytes = invalid(std::uint32_t{42});]=] TRUE)
compile_case(escaping_allocation [=[constexpr auto bytes = codec::make_binary_none_bytes<[] { return new models::scalar_node{42, true}; }>();]=] TRUE)
message(STATUS "Verified valid control and nine constant-evaluation failures")
