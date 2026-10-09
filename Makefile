# Keep these native build commands synchronized with make.ps1.
# CMake remains the source of all build rules.
.DEFAULT_GOAL := all

CONFIG ?= Release
BUILD_DIR ?= out/build/make-$(CONFIG)
JOBS ?= 4
CMAKE ?= cmake
CTEST ?= ctest
CMAKE_ARGS ?=

ifneq ($(strip $(VCPKG_ROOT)),)
VCPKG_ARGS = "-DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake"
endif

.PHONY: all configure test clean rebuild

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE=$(CONFIG) $(VCPKG_ARGS) $(CMAKE_ARGS)

all: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config $(CONFIG) --parallel $(JOBS)

clean:
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then \
		$(CMAKE) --build "$(BUILD_DIR)" --config $(CONFIG) --target clean; \
	else \
		printf '%s\n' "No configured build to clean: $(BUILD_DIR)"; \
	fi

# Let CMake sequence cleanup before compilation, including under make -j.
rebuild: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config $(CONFIG) --clean-first --parallel $(JOBS)

test: all
	$(CTEST) --test-dir "$(BUILD_DIR)" --build-config $(CONFIG) --output-on-failure
