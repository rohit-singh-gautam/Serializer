# An empty list is valid for static packages and examples without optional dependencies.
foreach(runtime_dll IN LISTS DLL_FILES)
  get_filename_component(name "${runtime_dll}" NAME)
  file(COPY_FILE "${runtime_dll}" "${OUTPUT_DIRECTORY}/${name}" ONLY_IF_DIFFERENT)
endforeach()
