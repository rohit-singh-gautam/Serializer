foreach(profile IN ITEMS serializer core google llvm gnu cert misra autosar qt)
  set(header_directory "${CMAKE_CURRENT_BINARY_DIR}/${profile}")
  set(header "${header_directory}/account.hpp")
  set(config "${CMAKE_CURRENT_SOURCE_DIR}/${profile}.ini")
  add_custom_command(
    OUTPUT "${header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${header_directory}"
    COMMAND serializer
      input "${CMAKE_CURRENT_SOURCE_DIR}/account.def" output "${header}" config "${config}"
      cpp.clang_format "${SERIALIZER_CLANG_FORMAT_EXECUTABLE}"
    DEPENDS serializer "${CMAKE_CURRENT_SOURCE_DIR}/account.def" "${config}"
    VERBATIM
  )
  set(target "serializer_style_${profile}")
  add_executable(${target} "${profile}.cpp" "${header}")
  target_link_libraries(${target} PRIVATE serializer_lib)
  target_include_directories(${target} PRIVATE "${header_directory}")
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  elseif(UNIX)
    target_compile_options(${target} PRIVATE -Wall -Werror -Wpedantic -Wextra)
  endif()
  if(SERIALIZER_BUILD_TESTS)
    add_test(NAME ${target} COMMAND ${target})
  endif()
endforeach()
