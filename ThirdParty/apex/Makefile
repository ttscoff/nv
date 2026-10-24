# Makefile for Apex
# Version bumping and release build tasks

SHELL := /bin/bash -o pipefail

.PHONY: bump bump-patch bump-minor bump-major help man build install release release-macos release-linux clean-release version

# Default bump type is patch
TYPE ?= patch

# Read current version from VERSION file
CURRENT_VERSION := $(shell cat VERSION 2>/dev/null || echo "0.1.0")

# Release configuration
RELEASE_DIR := release
VERSION := $(CURRENT_VERSION)
BUILD_DIR := build-release

# Code signing (set SIGNING_IDENTITY to your Apple Developer identity, or leave empty for ad-hoc)
# For ad-hoc signing (free, works for Homebrew): leave empty or set to "-"
# For proper signing (requires Apple Developer account): set to your identity (e.g., "Developer ID Application: Your Name")
SIGNING_IDENTITY ?= -

# Detect platform
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Parse version components
VERSION_PARTS := $(subst ., ,$(CURRENT_VERSION))
V_MAJOR := $(word 1,$(VERSION_PARTS))
V_MINOR := $(word 2,$(VERSION_PARTS))
V_PATCH := $(word 3,$(VERSION_PARTS))

# Calculate new version based on TYPE
ifeq ($(TYPE),major)
	NEW_MAJOR := $(shell echo $$(($(V_MAJOR) + 1)))
	NEW_MINOR := 0
	NEW_PATCH := 0
else ifeq ($(TYPE),minor)
	NEW_MAJOR := $(V_MAJOR)
	NEW_MINOR := $(shell echo $$(($(V_MINOR) + 1)))
	NEW_PATCH := 0
else
	# Default to patch
	NEW_MAJOR := $(V_MAJOR)
	NEW_MINOR := $(V_MINOR)
	NEW_PATCH := $(shell echo $$(($(V_PATCH) + 1)))
endif

NEW_VERSION := $(NEW_MAJOR).$(NEW_MINOR).$(NEW_PATCH)

# Default target: build from source
build:
	@echo "Configuring build with cmake..."
	@cmake -S . -B build
	@echo "Building..."
	@cmake --build build
	@echo "Build complete! Binary is in build/ directory."

# Print current version
version:
	@echo $(CURRENT_VERSION)

help:
	@echo "Apex Makefile"
	@echo ""
	@echo "Building:"
	@echo "  make build                  - Build from source (runs cmake configure and build)"
	@echo "  make install                - Install built binaries and libraries"
	@echo ""
	@echo "Version info:"
	@echo "  make version                - Print current version"
	@echo ""
	@echo "Version bumping:"
	@echo "  make bump [TYPE=patch]     - Bump version (patch/minor/major, defaults to patch)"
	@echo "  make bump-patch             - Bump patch version (0.1.0 -> 0.1.1)"
	@echo "  make bump-minor             - Bump minor version (0.1.0 -> 0.2.0)"
	@echo "  make bump-major             - Bump major version (0.1.0 -> 1.0.0)"
	@echo ""
	@echo "Documentation:"
	@echo "  make man                    - Generate man pages from Markdown (using apex -t man; runs build if needed)"
	@echo ""
	@echo "Release builds:"
	@echo "  make release                - Build release binary for current platform"
	@echo "  make release-macos          - Build macOS universal binary (arm64 + x86_64)"
	@echo "  make release-linux          - Build Linux binary for current architecture"
	@echo "  make clean-release          - Clean release build artifacts"
	@echo ""
	@echo "Code signing (macOS):"
	@echo "  make release SIGNING_IDENTITY=\"-\"           - Ad-hoc sign (free, for Homebrew)"
	@echo "  make release SIGNING_IDENTITY=\"Developer ID\" - Proper signing (requires cert)"
	@echo ""
	@echo "Note: Use 'make bump TYPE=patch' syntax (TYPE defaults to patch if omitted)"
	@echo "      Or use convenience targets: make bump-patch, make bump-minor, make bump-major"
	@echo ""
	@echo "Current version: $(CURRENT_VERSION)"

bump: check-version
	@echo "Bumping $(TYPE) version: $(CURRENT_VERSION) -> $(NEW_VERSION)"
	@echo "$(NEW_VERSION)" > VERSION
	@perl -i -pe 's/project\(apex VERSION \K[\d.]+/$(NEW_VERSION)/' CMakeLists.txt
	@perl -i -pe 's/#define APEX_VERSION_MAJOR \K\d+/$(NEW_MAJOR)/' include/apex/apex.h
	@perl -i -pe 's/#define APEX_VERSION_MINOR \K\d+/$(NEW_MINOR)/' include/apex/apex.h
	@perl -i -pe 's/#define APEX_VERSION_PATCH \K\d+/$(NEW_PATCH)/' include/apex/apex.h
	@perl -i -pe 's/#define APEX_VERSION_STRING "\K[\d.]+/$(NEW_VERSION)/' include/apex/apex.h
	@echo "Version bumped to $(NEW_VERSION)"

bump-patch:
	@$(MAKE) bump TYPE=patch

bump-minor:
	@$(MAKE) bump TYPE=minor

bump-major:
	@$(MAKE) bump TYPE=major

check-version:
	@if [ -z "$(CURRENT_VERSION)" ]; then \
		echo "Error: Could not read version from VERSION file"; \
		exit 1; \
	fi
	@if [ -z "$(NEW_VERSION)" ]; then \
		echo "Error: Could not calculate new version"; \
		exit 1; \
	fi

# Install built binaries and libraries
install: build
	@echo "Installing apex..."
	@cmake --install build
	@echo "Installation complete!"

# Man page generation (uses built apex with -t man)
# Empty XDG_CONFIG_HOME avoids user-installed plugins (can break -t man); UTF-8 for JSON in plugins.
man: build
	@echo "Generating man pages with apex -t man..."
	@mkdir -p build/empty-xdg-for-man && LANG=en_US.UTF-8 XDG_CONFIG_HOME="$(CURDIR)/build/empty-xdg-for-man" ./build/apex -t man --no-indices man/apex.1.md > man/apex.1 && echo "Man page generated: man/apex.1"
	@if [ -f man/apex-config.5.md ]; then LANG=en_US.UTF-8 XDG_CONFIG_HOME="$(CURDIR)/build/empty-xdg-for-man" ./build/apex -t man --no-indices man/apex-config.5.md > man/apex-config.5 && echo "Man page generated: man/apex-config.5"; fi
	@if [ -f man/apex-plugins.7.md ]; then LANG=en_US.UTF-8 XDG_CONFIG_HOME="$(CURDIR)/build/empty-xdg-for-man" ./build/apex -t man --no-indices man/apex-plugins.7.md > man/apex-plugins.7 && echo "Man page generated: man/apex-plugins.7"; fi

# Release build targets
release: clean-release
	@echo "Building release binaries for version $(VERSION)"
	@echo "Generating man page for release..."
	@$(MAKE) man || (echo "Warning: Could not generate man page. Continuing anyway..."; true)
	@mkdir -p $(RELEASE_DIR)
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
		$(MAKE) release-macos; \
	elif [ "$(UNAME_S)" = "Linux" ]; then \
		$(MAKE) release-linux; \
	else \
		echo "Unsupported platform: $(UNAME_S)"; \
		exit 1; \
	fi

release-macos:
	@echo "Building macOS release for version $(VERSION)..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DBUILD_FRAMEWORK=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev ..
	@cd $(BUILD_DIR) && $(MAKE) -j$$(sysctl -n hw.ncpu) apex_cli
	@mkdir -p $(RELEASE_DIR)/apex-$(VERSION)-macos-universal
	@cp $(BUILD_DIR)/apex $(RELEASE_DIR)/apex-$(VERSION)-macos-universal/apex
	@for f in apex.1 apex-config.5 apex-plugins.7; do \
		if [ -f man/$$f ]; then \
			cp man/$$f $(RELEASE_DIR)/apex-$(VERSION)-macos-universal/; \
			echo "  Bundled man page: $$f"; \
		else \
			echo "  Warning: man/$$f missing; not included in release archive"; \
		fi; \
	done
	@echo "Fixing libyaml library path to use @rpath for portability..."
	@if otool -L $(RELEASE_DIR)/apex-$(VERSION)-macos-universal/apex | grep -q "libyaml.*dylib"; then \
		echo "  Adding rpaths for common libyaml locations..."; \
		install_name_tool -add_rpath /usr/local/lib $(RELEASE_DIR)/apex-$(VERSION)-macos-universal/apex 2>/dev/null || true; \
		install_name_tool -add_rpath /opt/homebrew/lib $(RELEASE_DIR)/apex-$(VERSION)-macos-universal/apex 2>/dev/null || true; \
		echo "  Changing libyaml path to use @rpath..."; \
		install_name_tool -change "/Users/runner/work/apex/apex/deps/libyaml-universal/lib/libyaml-0.2.dylib" "@rpath/libyaml-0.2.dylib" $(RELEASE_DIR)/apex-$(VERSION)-macos-universal/apex 2>/dev/null || true; \
		install_name_tool -change "/opt/homebrew/opt/libyaml/lib/libyaml-0.2.dylib" "@rpath/libyaml-0.2.dylib" $(RELEASE_DIR)/apex-$(VERSION)-macos-universal/apex 2>/dev/null || true; \
		install_name_tool -change "/usr/local/opt/libyaml/lib/libyaml-0.2.dylib" "@rpath/libyaml-0.2.dylib" $(RELEASE_DIR)/apex-$(VERSION)-macos-universal/apex 2>/dev/null || true; \
		echo "  libyaml now uses @rpath - binary will look for libyaml in /usr/local/lib and /opt/homebrew/lib"; \
	else \
		echo "  No libyaml found, skipping fix"; \
	fi
	@if [ -n "$(SIGNING_IDENTITY)" ] && [ "$(SIGNING_IDENTITY)" != "none" ]; then \
		echo "Signing binary with identity: $(SIGNING_IDENTITY)..."; \
		codesign --force --sign "$(SIGNING_IDENTITY)" --timestamp --options runtime $(RELEASE_DIR)/apex-$(VERSION)-macos-universal/apex || echo "Warning: Code signing failed (this is OK for ad-hoc signing)"; \
	fi
	@cd $(RELEASE_DIR) && tar -czf apex-$(VERSION)-macos-universal.tar.gz apex-$(VERSION)-macos-universal/
	@echo "macOS release built: $(RELEASE_DIR)/apex-$(VERSION)-macos-universal.tar.gz"
	@echo "SHA256: $$(cd $(RELEASE_DIR) && shasum -a 256 apex-$(VERSION)-macos-universal.tar.gz | cut -d' ' -f1)"

release-linux:
	@echo "Building Linux release for version $(VERSION)..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev ..
	@cd $(BUILD_DIR) && $(MAKE) -j$$(nproc) apex_cli
	@mkdir -p $(RELEASE_DIR)/apex-$(VERSION)-linux-$(UNAME_M)
	@cp $(BUILD_DIR)/apex $(RELEASE_DIR)/apex-$(VERSION)-linux-$(UNAME_M)/apex
	@for f in apex.1 apex-config.5 apex-plugins.7; do \
		if [ -f man/$$f ]; then \
			cp man/$$f $(RELEASE_DIR)/apex-$(VERSION)-linux-$(UNAME_M)/; \
			echo "  Bundled man page: $$f"; \
		else \
			echo "  Warning: man/$$f missing; not included in release archive"; \
		fi; \
	done
	@cd $(RELEASE_DIR) && tar -czf apex-$(VERSION)-linux-$(UNAME_M).tar.gz apex-$(VERSION)-linux-$(UNAME_M)/
	@echo "Linux release built: $(RELEASE_DIR)/apex-$(VERSION)-linux-$(UNAME_M).tar.gz"
	@echo "SHA256: $$(cd $(RELEASE_DIR) && sha256sum apex-$(VERSION)-linux-$(UNAME_M).tar.gz | cut -d' ' -f1)"

clean-release:
	@echo "Cleaning release build..."
	@rm -rf $(BUILD_DIR) $(RELEASE_DIR)
	@echo "Clean complete"

