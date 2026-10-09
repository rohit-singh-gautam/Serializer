# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later

# Stage the selected configuration's imported runtime dependencies beside a test executable.
# Static-only targets provide an empty list and need no files copied.
if(NOT DEFINED OUTPUT_DIRECTORY OR OUTPUT_DIRECTORY STREQUAL "")
  message(FATAL_ERROR "Runtime DLL staging requires OUTPUT_DIRECTORY")
endif()
foreach(runtime_dll IN LISTS DLL_FILES)
  get_filename_component(runtime_dll_name "${runtime_dll}" NAME)
  file(COPY_FILE "${runtime_dll}" "${OUTPUT_DIRECTORY}/${runtime_dll_name}" ONLY_IF_DIFFERENT)
endforeach()
