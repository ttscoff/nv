# Package support files

Headers and configuration used when building Apex via **Swift Package Manager** (SPM), e.g. when adding Apex as an integrated Xcode package.

## cmark-gfm

The `cmark-gfm/` directory provides generated headers that cmark-gfm normally gets from its CMake build:

- **config.h** – Feature detection (e.g. `HAVE_STDBOOL_H`, `HAVE___ATTRIBUTE__`). CMake would produce this from `config.h.in`.
- **cmark-gfm_export.h** – Symbol visibility macros (`CMARK_GFM_EXPORT`). When `CMARK_GFM_STATIC_DEFINE` is set (always for SPM), these expand to nothing.
- **cmark-gfm_version.h** – Version constants used by the library. Must match `vendor/cmark-gfm` CMakeLists.txt.

Without these, SPM builds fail with errors like “missing config.h” or “missing cmark-gfm_export.h” because no CMake run occurs.

The `CcmarkGFMSupport` target (see `Package.swift`) owns this directory: it builds a trivial stub and exposes these headers as public. CcmarkGFM and CcmarkGFMExtensions depend on CcmarkGFMSupport, so they receive the include path via **dependency** headers rather than `headerSearchPath`. That matters because SPM does not apply `headerSearchPath` when the package is built as a dependency (e.g. in Xcode); using a dependency ensures the headers are found.

See [issue #7](https://github.com/ApexMarkdown/apex/issues/7).
