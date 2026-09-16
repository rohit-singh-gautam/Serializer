foreach(profile IN ITEMS serializer core google llvm gnu cert misra autosar qt)
  set(target "serializer_style_${profile}")
  add_executable(${target} "${profile}/${profile}.cpp")
  serializer_generate(TARGET ${target}
    SCHEMAS "${profile}/account.def"
    CONFIG "${profile}/${profile}.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${profile}")
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  elseif(UNIX)
    target_compile_options(${target} PRIVATE -Wall -Werror -Wpedantic -Wextra)
  endif()
  if(SERIALIZER_BUILD_TESTS)
    add_test(NAME ${target} COMMAND ${target})
  endif()
endforeach()
