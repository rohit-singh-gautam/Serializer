# Keep these native build commands synchronized with make.ps1.
# CMake remains the source of native build rules; Git owns repository cleanup.
.DEFAULT_GOAL := all

CONFIG ?= Release
BUILD_DIR ?= out/build/make-$(CONFIG)
JOBS ?= 4
CMAKE ?= cmake
CTEST ?= ctest
CMAKE_ARGS ?=
MAKEFILE_DIRECTORY := $(realpath $(dir $(firstword $(MAKEFILE_LIST))))

ifneq ($(strip $(VCPKG_ROOT)),)
VCPKG_ARGS = "-DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake"
endif

.PHONY: all configure test clean rebuild extension

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE=$(CONFIG) $(VCPKG_ARGS) $(CMAKE_ARGS)

all: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config $(CONFIG) --parallel $(JOBS)

clean:
	@repository_root=$$(git -C "$(MAKEFILE_DIRECTORY)" rev-parse --show-toplevel) && \
	if [ "$$repository_root" != "$(MAKEFILE_DIRECTORY)" ]; then \
		printf '%s\n' "Refusing to clean outside the repository containing this Makefile." >&2; \
		exit 1; \
	fi && \
	git -C "$$repository_root" clean -fdx

# Keep repository cleanup ahead of configuration, including under make -j.
rebuild:
	$(MAKE) --file="$(firstword $(MAKEFILE_LIST))" clean
	$(MAKE) --file="$(firstword $(MAKEFILE_LIST))" configure
	$(CMAKE) --build "$(BUILD_DIR)" --config $(CONFIG) --clean-first --parallel $(JOBS)

# WSL delegates both VSIX builds to the existing Windows packaging pipeline.
extension:
	@if command -v powershell.exe >/dev/null 2>&1 && command -v wslpath >/dev/null 2>&1; then \
		script_path=$$(wslpath -w "$(MAKEFILE_DIRECTORY)/editors/build.ps1") && \
		powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$$script_path"; \
	else \
		printf '%s\n' "Building both extensions requires Windows and Visual Studio MSBuild." \
			"Run ./make.ps1 extension on Windows, or make extension from WSL with Windows interop." >&2; \
		exit 1; \
	fi

test: all
	$(CTEST) --test-dir "$(BUILD_DIR)" --build-config $(CONFIG) --output-on-failure
