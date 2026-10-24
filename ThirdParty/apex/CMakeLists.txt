cmake_minimum_required(VERSION 3.10)
# Set policy version minimum for compatibility with newer CMake versions
# This is especially important for cmark-gfm subdirectory
if(POLICY CMP0000)
    cmake_policy(SET CMP0000 NEW)
endif()
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum CMake policy version" FORCE)

project(apex VERSION 1.1.47 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3)
    endif()
endif()

# Find libyaml for YAML parsing (arrays, nested structures)
#
# We try a few common pkg-config names in order:
#   - yaml-0.1  (Homebrew / many distros)
#   - yaml      (some distros)
#   - libyaml   (fallback)
#
# If that fails, we fall back to find_package(yaml).
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(YAML yaml-0.1 QUIET)
    if(NOT YAML_FOUND)
        pkg_check_modules(YAML yaml QUIET)
    endif()
    if(NOT YAML_FOUND)
        pkg_check_modules(YAML libyaml QUIET)
    endif()
endif()

if(NOT YAML_FOUND)
    find_package(yaml QUIET)
    if(yaml_FOUND)
        set(YAML_LIBRARIES yaml)
        set(YAML_INCLUDE_DIRS ${yaml_INCLUDE_DIRS})
        set(YAML_FOUND TRUE)
    endif()
endif()

if(NOT YAML_FOUND)
    message(WARNING "libyaml not found. YAML arrays and nested structures will not be supported.")
    message(STATUS "  Install libyaml-dev (Debian/Ubuntu) or libyaml-devel (RHEL/Fedora) or libyaml (Homebrew)")
    set(YAML_FOUND FALSE)
else()
    message(STATUS "libyaml: Found")
    message(STATUS "  Libraries: ${YAML_LIBRARIES}")
    message(STATUS "  Include dirs: ${YAML_INCLUDE_DIRS}")
endif()

# Build cmark-gfm as a subdirectory
# Disable cmark-gfm tests and programs
set(CMARK_TESTS OFF CACHE BOOL "Build cmark tests" FORCE)
set(CMARK_STATIC ON CACHE BOOL "Build static library" FORCE)
set(CMARK_SHARED ON CACHE BOOL "Build shared library" FORCE)
add_subdirectory(vendor/cmark-gfm)

# Silence noisy warnings from vendored cmark-gfm extensions
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    if(TARGET libcmark-gfm-extensions)
        target_compile_options(libcmark-gfm-extensions PRIVATE -Wno-unused-parameter -Wno-c99-extensions)
    endif()
    if(TARGET libcmark-gfm-extensions_static)
        target_compile_options(libcmark-gfm-extensions_static PRIVATE -Wno-unused-parameter -Wno-c99-extensions)
    endif()
    # Also suppress c99-extensions and unused-parameter warnings for cmark-gfm library targets
    # The project name "cmark-gfm" creates an invalid macro #define cmark-gfm 1
    if(TARGET libcmark-gfm)
        target_compile_options(libcmark-gfm PRIVATE -Wno-c99-extensions -Wno-unused-parameter)
    endif()
    if(TARGET libcmark-gfm_static)
        target_compile_options(libcmark-gfm_static PRIVATE -Wno-c99-extensions -Wno-unused-parameter)
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/vendor/cmark-gfm/src
    ${CMAKE_SOURCE_DIR}/vendor/cmark-gfm/extensions
    ${CMAKE_BINARY_DIR}/vendor/cmark-gfm/src  # For generated headers
)

if(YAML_FOUND)
    include_directories(${YAML_INCLUDE_DIRS})
    link_directories(${YAML_LIBRARY_DIRS})
    add_compile_definitions(APEX_HAVE_LIBYAML=1)
endif()


# Library source files
set(APEX_LIB_SOURCES
    src/apex.c
    src/ast_json.c
    src/ast_markdown.c
    src/ast_terminal.c
    src/ast_man.c
    src/ast_rtf.c
    src/filters_ast.c
    src/plugins_env.c
    src/plugins.c
    src/plugins_remote.c
    src/plugin_catalog.c
    src/html_renderer.c
    src/extensions/metadata.c
    src/extensions/wiki_links.c
    src/extensions/math.c
    src/extensions/critic.c
    src/extensions/callouts.c
    src/extensions/raw_content.c
    src/extensions/code_fence_attrs.c
    src/extensions/quarto_diagrams.c
    src/extensions/quarto_shortcodes.c
    src/extensions/quarto_polish.c
    src/extensions/quarto_lists.c
    src/extensions/includes.c
    src/extensions/inline_tables.c
    src/extensions/toc.c
    src/extensions/abbreviations.c
    src/extensions/emoji.c
    src/extensions/special_markers.c
    src/extensions/ial.c
    src/extensions/bear_image_attrs.c
    src/extensions/definition_list.c
    src/extensions/advanced_footnotes.c
    src/extensions/advanced_tables.c
    src/extensions/html_markdown.c
    src/extensions/fenced_divs.c
    src/extensions/table_html_postprocess.c
    src/extensions/inline_footnotes.c
    src/extensions/highlight.c
    src/extensions/insert.c
    src/extensions/sup_sub.c
    src/extensions/header_ids.c
    src/extensions/relaxed_tables.c
    src/extensions/grid_tables.c
    src/extensions/citations.c
    src/extensions/index.c
    src/extensions/concordance.c
    src/extensions/syntax_highlight.c
    src/pretty_html.c
)

# Build shared library
add_library(apex SHARED ${APEX_LIB_SOURCES})
set_target_properties(apex PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER include/apex/apex.h
)
if(YAML_FOUND)
    target_link_libraries(apex libcmark-gfm-extensions libcmark-gfm ${YAML_LIBRARIES})
else()
    target_link_libraries(apex libcmark-gfm-extensions libcmark-gfm)
endif()

# Build static library
add_library(apex_static STATIC ${APEX_LIB_SOURCES})
set_target_properties(apex_static PROPERTIES
    OUTPUT_NAME apex
    PUBLIC_HEADER include/apex/apex.h
)
if(YAML_FOUND)
    target_link_libraries(apex_static libcmark-gfm-extensions_static libcmark-gfm_static ${YAML_LIBRARIES})
else()
    target_link_libraries(apex_static libcmark-gfm-extensions_static libcmark-gfm_static)
endif()

# CLI executable
add_executable(apex_cli cli/main.c)
set_target_properties(apex_cli PROPERTIES OUTPUT_NAME apex)
if(YAML_FOUND)
    target_link_libraries(apex_cli apex_static libcmark-gfm-extensions_static libcmark-gfm_static ${YAML_LIBRARIES})
    # On macOS, change libyaml library path to use @rpath so it can be found in standard locations
    # This allows the binary to work when copied to /usr/local/bin without requiring Homebrew
    if(APPLE)
        find_program(INSTALL_NAME_TOOL install_name_tool)
        if(INSTALL_NAME_TOOL)
            add_custom_command(TARGET apex_cli POST_BUILD
                # Add rpaths for common libyaml locations
                COMMAND ${INSTALL_NAME_TOOL} -add_rpath /usr/local/lib $<TARGET_FILE:apex_cli> 2>/dev/null || true
                COMMAND ${INSTALL_NAME_TOOL} -add_rpath /opt/homebrew/lib $<TARGET_FILE:apex_cli> 2>/dev/null || true
                # Change libyaml path from absolute to @rpath
                # Try common Homebrew paths first
                COMMAND ${INSTALL_NAME_TOOL} -change "/opt/homebrew/opt/libyaml/lib/libyaml-0.2.dylib" "@rpath/libyaml-0.2.dylib" $<TARGET_FILE:apex_cli> 2>/dev/null || true
                COMMAND ${INSTALL_NAME_TOOL} -change "/usr/local/opt/libyaml/lib/libyaml-0.2.dylib" "@rpath/libyaml-0.2.dylib" $<TARGET_FILE:apex_cli> 2>/dev/null || true
                # Handle build-time paths from CI/CD
                COMMAND ${INSTALL_NAME_TOOL} -change "/Users/runner/work/apex/apex/deps/libyaml-universal/lib/libyaml-0.2.dylib" "@rpath/libyaml-0.2.dylib" $<TARGET_FILE:apex_cli> 2>/dev/null || true
                COMMENT "Setting up rpaths and changing libyaml to use @rpath"
            )
        endif()
    endif()
else()
    target_link_libraries(apex_cli apex_static libcmark-gfm-extensions_static libcmark-gfm_static)
endif()

# Installation
install(TARGETS apex apex_static apex_cli
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/apex
)

# macOS Framework (optional, when building on macOS)
if(APPLE)
    option(BUILD_FRAMEWORK "Build macOS Framework" ON)
    if(BUILD_FRAMEWORK)
        add_library(apex_framework SHARED ${APEX_LIB_SOURCES})
        set_target_properties(apex_framework PROPERTIES
            FRAMEWORK TRUE
            FRAMEWORK_VERSION A
            MACOSX_FRAMEWORK_IDENTIFIER com.brettterpstra.apex
            MACOSX_FRAMEWORK_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist.in
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
            PUBLIC_HEADER include/apex/apex.h
            OUTPUT_NAME Apex
        )
        # Link cmark statically so an embedded framework does not depend on
        # libcmark-gfm*.dylib paths from the build tree.
        if(TARGET libcmark-gfm_static AND TARGET libcmark-gfm-extensions_static)
            target_link_libraries(apex_framework libcmark-gfm-extensions_static libcmark-gfm_static)
        else()
            target_link_libraries(apex_framework libcmark-gfm libcmark-gfm-extensions)
        endif()

        # libyaml resolves to an absolute Homebrew path that does not exist on
        # end-user machines, so app-embedded builds can opt out of it. YAML
        # array support is unavailable in the framework when this is ON.
        option(APEX_FRAMEWORK_NO_YAML "Build Apex.framework without libyaml" OFF)
        if(YAML_FOUND AND NOT APEX_FRAMEWORK_NO_YAML)
            target_link_libraries(apex_framework ${YAML_LIBRARIES})
        elseif(YAML_FOUND)
            # Sources test #ifdef APEX_HAVE_LIBYAML; compile options follow the
            # directory-level -D, so -U here removes it for this target only.
            target_compile_options(apex_framework PRIVATE -UAPEX_HAVE_LIBYAML)
        endif()

        # Install framework
        # Use absolute path /Library/Frameworks (standard macOS framework location)
        # Users can override with CMAKE_INSTALL_PREFIX if needed
        if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
            set(FRAMEWORK_INSTALL_DIR "/Library/Frameworks")
        else()
            set(FRAMEWORK_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/Library/Frameworks")
        endif()

        install(TARGETS apex_framework
            FRAMEWORK DESTINATION ${FRAMEWORK_INSTALL_DIR}
            PUBLIC_HEADER DESTINATION ${FRAMEWORK_INSTALL_DIR}/Apex.framework/Headers
        )

        # Explicitly install the header file into the framework Headers directory
        # (PUBLIC_HEADER doesn't automatically install into frameworks)
        install(FILES include/apex/apex.h
            DESTINATION ${FRAMEWORK_INSTALL_DIR}/Apex.framework/Headers
        )

        # Install module map for Swift support
        install(FILES include/apex/module.modulemap
            DESTINATION ${FRAMEWORK_INSTALL_DIR}/Apex.framework/Headers
        )

        # Post-install script to copy dylibs into framework and fix install names
        # This runs after the framework is installed
        install(CODE "
            # Determine framework location (same logic as above)
            if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR CMAKE_INSTALL_PREFIX STREQUAL \"/usr/local\")
                set(FRAMEWORK_INSTALL_DIR \"/Library/Frameworks\")
            else()
                set(FRAMEWORK_INSTALL_DIR \"\${CMAKE_INSTALL_PREFIX}/Library/Frameworks\")
            endif()
            set(FRAMEWORK_DIR \"\${FRAMEWORK_INSTALL_DIR}/Apex.framework\")
            set(FRAMEWORK_FRAMEWORKS_DIR \"\${FRAMEWORK_DIR}/Frameworks\")
            set(FRAMEWORK_BINARY \"\${FRAMEWORK_DIR}/Apex\")
            set(BUILD_DIR \"${CMAKE_BINARY_DIR}\")

            # Create Frameworks directory inside the framework
            file(MAKE_DIRECTORY \"\${FRAMEWORK_FRAMEWORKS_DIR}\")

            # Find dylibs in build directory
            file(GLOB CMARK_GFM_DYLIB_BUILD \"\${BUILD_DIR}/vendor/cmark-gfm/src/libcmark-gfm*.dylib\")
            file(GLOB CMARK_GFM_EXTENSIONS_DYLIB_BUILD \"\${BUILD_DIR}/vendor/cmark-gfm/extensions/libcmark-gfm-extensions*.dylib\")

            # Copy and fix cmark-gfm dylib
            if(CMARK_GFM_DYLIB_BUILD)
                # Get first file from glob result (in case multiple files found)
                list(GET CMARK_GFM_DYLIB_BUILD 0 CMARK_GFM_DYLIB_SINGLE)
                get_filename_component(CMARK_GFM_NAME \"\${CMARK_GFM_DYLIB_SINGLE}\" NAME)
                set(CMARK_GFM_DEST \"\${FRAMEWORK_FRAMEWORKS_DIR}/\${CMARK_GFM_NAME}\")
                file(COPY \"\${CMARK_GFM_DYLIB_SINGLE}\" DESTINATION \"\${FRAMEWORK_FRAMEWORKS_DIR}\")
                # Fix install name
                execute_process(
                    COMMAND install_name_tool -id \"@rpath/Apex.framework/Frameworks/\${CMARK_GFM_NAME}\" \"\${CMARK_GFM_DEST}\"
                    RESULT_VARIABLE RESULT
                    ERROR_QUIET
                )
                # Fix framework binary reference
                execute_process(
                    COMMAND install_name_tool -change \"@rpath/\${CMARK_GFM_NAME}\" \"@rpath/Apex.framework/Frameworks/\${CMARK_GFM_NAME}\" \"\${FRAMEWORK_BINARY}\"
                    RESULT_VARIABLE RESULT
                    ERROR_QUIET
                )
            endif()

            # Copy and fix cmark-gfm-extensions dylib
            if(CMARK_GFM_EXTENSIONS_DYLIB_BUILD)
                # Get first file from glob result (in case multiple files found)
                list(GET CMARK_GFM_EXTENSIONS_DYLIB_BUILD 0 CMARK_GFM_EXTENSIONS_DYLIB_SINGLE)
                get_filename_component(CMARK_GFM_EXTENSIONS_NAME \"\${CMARK_GFM_EXTENSIONS_DYLIB_SINGLE}\" NAME)
                set(CMARK_GFM_EXTENSIONS_DEST \"\${FRAMEWORK_FRAMEWORKS_DIR}/\${CMARK_GFM_EXTENSIONS_NAME}\")
                file(COPY \"\${CMARK_GFM_EXTENSIONS_DYLIB_SINGLE}\" DESTINATION \"\${FRAMEWORK_FRAMEWORKS_DIR}\")
                # Fix install name
                execute_process(
                    COMMAND install_name_tool -id \"@rpath/Apex.framework/Frameworks/\${CMARK_GFM_EXTENSIONS_NAME}\" \"\${CMARK_GFM_EXTENSIONS_DEST}\"
                    RESULT_VARIABLE RESULT
                    ERROR_QUIET
                )
                # Fix framework binary reference
                execute_process(
                    COMMAND install_name_tool -change \"@rpath/\${CMARK_GFM_EXTENSIONS_NAME}\" \"@rpath/Apex.framework/Frameworks/\${CMARK_GFM_EXTENSIONS_NAME}\" \"\${FRAMEWORK_BINARY}\"
                    RESULT_VARIABLE RESULT
                    ERROR_QUIET
                )
                # Fix cmark-gfm-extensions to reference cmark-gfm from within framework
                if(CMARK_GFM_DYLIB_BUILD)
                    # Get first file from glob result (in case multiple files found)
                    list(GET CMARK_GFM_DYLIB_BUILD 0 CMARK_GFM_DYLIB_SINGLE)
                    get_filename_component(CMARK_GFM_NAME \"\${CMARK_GFM_DYLIB_SINGLE}\" NAME)
                    execute_process(
                        COMMAND install_name_tool -change \"@rpath/\${CMARK_GFM_NAME}\" \"@rpath/Apex.framework/Frameworks/\${CMARK_GFM_NAME}\" \"\${CMARK_GFM_EXTENSIONS_DEST}\"
                        RESULT_VARIABLE RESULT
                        ERROR_QUIET
                    )
                endif()
            endif()
        ")
    endif()
endif()

# Testing
enable_testing()

# Test runner executable
add_executable(apex_test_runner
    tests/test_helpers.c
    tests/test_runner.c
    tests/test_basic.c
    tests/test_cmark_callback.c
    tests/test_custom_plugins.c
    tests/test_metadata.c
    tests/test_links.c
    tests/test_tables.c
    tests/test_ial.c
    tests/test_bear_image_attrs.c
    tests/test_extensions.c
    tests/test_output.c
    tests/test_marked_integration.c
    tests/test_syntax_highlight.c
    tests/test_plugins.c
    tests/test_escaping_repro.c
)
target_link_libraries(apex_test_runner apex_static)
target_compile_definitions(apex_test_runner PRIVATE TEST_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/includes")

# Add test (run from source dir so fixture paths like tests/fixtures/... resolve)
add_test(NAME apex_tests COMMAND apex_test_runner)
set_tests_properties(apex_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Documentation
option(BUILD_DOCS "Build documentation" OFF)
if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
        )
    endif()
endif()

# Man page handling
# Define all man page sources and their sections
set(MAN_PAGES
    "apex.1.md:1:apex"
    "apex-config.5.md:5:apex-config"
    "apex-plugins.7.md:7:apex-plugins"
)

# Find man page generation tools
find_program(PANDOC_EXECUTABLE pandoc)
find_program(GO_MD2MAN_EXECUTABLE go-md2man)

set(MAN_AVAILABLE FALSE)
set(MAN_OUTPUTS "")

# Process each man page
foreach(MAN_PAGE_SPEC ${MAN_PAGES})
    string(REPLACE ":" ";" MAN_PAGE_PARTS ${MAN_PAGE_SPEC})
    list(GET MAN_PAGE_PARTS 0 MAN_SOURCE_NAME)
    list(GET MAN_PAGE_PARTS 1 MAN_SECTION)
    list(GET MAN_PAGE_PARTS 2 MAN_NAME)

    set(MAN_SOURCE ${CMAKE_SOURCE_DIR}/man/${MAN_SOURCE_NAME})
    set(MAN_PREGENERATED ${CMAKE_SOURCE_DIR}/man/${MAN_NAME}.${MAN_SECTION})
    set(MAN_OUTPUT ${CMAKE_BINARY_DIR}/${MAN_NAME}.${MAN_SECTION})

    list(APPEND MAN_OUTPUTS ${MAN_OUTPUT})

    # Check if pre-generated man page exists
    if(EXISTS ${MAN_PREGENERATED})
        # Use pre-generated man page (copy to build directory for installation)
        add_custom_command(
            OUTPUT ${MAN_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E copy ${MAN_PREGENERATED} ${MAN_OUTPUT}
            DEPENDS ${MAN_PREGENERATED}
            COMMENT "Using pre-generated man/${MAN_NAME}.${MAN_SECTION}"
        )
        set(MAN_AVAILABLE TRUE)
    else()
        # Generate with apex -t man (roff); we build apex_cli in this tree
        add_custom_command(
            OUTPUT ${MAN_OUTPUT}
            COMMAND $<TARGET_FILE:apex_cli> -t man ${MAN_SOURCE} > ${MAN_OUTPUT}
            DEPENDS ${MAN_SOURCE} apex_cli
            COMMENT "Generating man page ${MAN_NAME}.${MAN_SECTION} from Markdown using apex -t man"
        )
        set(MAN_AVAILABLE TRUE)
    endif()

    # Install man page
    if(MAN_AVAILABLE)
        install(FILES ${MAN_OUTPUT}
            DESTINATION share/man/man${MAN_SECTION}
            RENAME ${MAN_NAME}.${MAN_SECTION}
        )
    endif()
endforeach()

# Create a target that depends on all man pages
if(MAN_AVAILABLE)
    add_custom_target(man_pages ALL DEPENDS ${MAN_OUTPUTS})
    message(STATUS "Man pages: Will be built from Markdown (apex -t man) or copied if pre-generated")
else()
    message(WARNING "Man page sources or pre-generated pages not found.")
    message(STATUS "  Man pages will not be installed.")
endif()

add_custom_target(swift_package_check
  COMMAND ${CMAKE_COMMAND} -E env
          SWIFT_PACKAGE_PATH=${CMAKE_SOURCE_DIR}
          swift build
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Building Swift package (debug)"
)

add_custom_target(swift_package_check_release
  COMMAND ${CMAKE_COMMAND} -E env
          SWIFT_PACKAGE_PATH=${CMAKE_SOURCE_DIR}
          swift build -c release
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Building Swift package (release)"
)

add_custom_target(all_checks
  DEPENDS apex_test_runner swift_package_check swift_package_check_release
)

# Print configuration
message(STATUS "Apex ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
if(APPLE AND BUILD_FRAMEWORK)
    message(STATUS "macOS Framework: Enabled")
endif()
