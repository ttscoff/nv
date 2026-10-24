// swift-tools-version:5.7
// The swift-tools-version declares the minimum version of Swift required to build this package.

import Foundation
import PackageDescription

/// Clang discovers `include/apex/module.modulemap` next to headers and builds a precompiled
/// `ApexHeaders` module, but SPM only loads `include/module.modulemap` (ApexC). Without passing
/// this path explicitly, `ApexObjC`/`Apex` fail with: module 'ApexHeaders' is not defined in any
/// loaded module map file.
private let apexHeadersModuleMap = URL(fileURLWithPath: #filePath)
    .deletingLastPathComponent()
    .appendingPathComponent("include/apex/module.modulemap")
    .path

let package = Package(
    name: "Apex",
    platforms: [
        .macOS(.v10_13),
        .iOS(.v11),
    ],
    products: [
        .library(
            name: "Apex",
            targets: ["Apex"]
        ),
        .library(
            name: "ApexC",
            targets: ["ApexC"]
        )
    ],
    targets: [
        // Headers for cmark-gfm SPM build (config.h, cmark-gfm_export.h, cmark-gfm_version.h).
        // Separate target so dependency include path is used; headerSearchPath is not
        // applied when the package is built as a dependency (Xcode/SPM bug).
        .target(
            name: "CcmarkGFMSupport",
            path: "PackageSupport/cmark-gfm",
            sources: ["cmark_gfm_spm_stub.c"],
            publicHeadersPath: ".",
            cSettings: [.headerSearchPath(".")]
        ),
        // cmark-gfm core library (built from submodule)
        .target(
            name: "CcmarkGFM",
            dependencies: ["CcmarkGFMSupport"],
            path: "vendor/cmark-gfm/src",
            exclude: [
                "main.c",  // CLI executable
                "xml.c",  // XML renderer (not needed)
                "man.c",  // Man page renderer (not needed)
                "plaintext.c",  // Plaintext renderer (not needed)
                "latex.c",  // LaTeX renderer (not needed)
            ],
            publicHeadersPath: ".",
            cSettings: [
                .headerSearchPath("."),
                .headerSearchPath("../extensions"),
                .define("CMARK_STATIC_DEFINE"),
                .define("CMARK_GFM_STATIC_DEFINE"),
                .define("HAVE_STDBOOL_H", to: "1"),
                .define("HAVE___BUILTIN_EXPECT", to: "1"),
                .define("HAVE___ATTRIBUTE__", to: "1"),
            ]
        ),
        // cmark-gfm extensions
        .target(
            name: "CcmarkGFMExtensions",
            dependencies: ["CcmarkGFM", "CcmarkGFMSupport"],
            path: "vendor/cmark-gfm/extensions",
            publicHeadersPath: ".",
            cSettings: [
                .headerSearchPath("../src"),
                .define("CMARK_GFM_EXTENSIONS_STATIC_DEFINE"),
            ]
        ),
        // libyaml bundled library (built from submodule)
        .target(
            name: "CYaml",
            path: "vendor/libyaml",
            exclude: [
                "src/Makefile.am"
            ],
            sources: ["src"],
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("include"),
                .headerSearchPath("src"),
                .define("YAML_VERSION_MAJOR", to: "0"),
                .define("YAML_VERSION_MINOR", to: "2"),
                .define("YAML_VERSION_PATCH", to: "5"),
                .define("YAML_VERSION_STRING", to: "\"0.2.5\""),
                // Upstream libyaml uses int for node indices/lengths on 64-bit; silence noise.
                .unsafeFlags(["-Wno-shorten-64-to-32"]),
            ]
        ),
        // Apex C library
        .target(
            name: "ApexC",
            dependencies: ["CcmarkGFM", "CcmarkGFMExtensions", "CYaml"],
            path: ".",
            sources: [
                "src/apex.c",
                "src/ast_markdown.c",
                "src/ast_terminal.c",
                "src/ast_man.c",
                "src/ast_rtf.c",
                "src/filters_ast.c",
                "src/ast_json.c",
                "src/plugins_env.c",
                "src/plugins.c",
                "src/plugins_remote.c",
                "src/plugin_catalog.c",
                "src/html_renderer.c",
                "src/extensions/metadata.c",
                "src/extensions/wiki_links.c",
                "src/extensions/math.c",
                "src/extensions/critic.c",
                "src/extensions/callouts.c",
                "src/extensions/raw_content.c",
                "src/extensions/code_fence_attrs.c",
                "src/extensions/quarto_diagrams.c",
                "src/extensions/quarto_shortcodes.c",
                "src/extensions/quarto_polish.c",
                "src/extensions/quarto_lists.c",
                "src/extensions/includes.c",
                "src/extensions/inline_tables.c",
                "src/extensions/toc.c",
                "src/extensions/abbreviations.c",
                "src/extensions/emoji.c",
                "src/extensions/special_markers.c",
                "src/extensions/ial.c",
                "src/extensions/bear_image_attrs.c",
                "src/extensions/definition_list.c",
                "src/extensions/advanced_footnotes.c",
                "src/extensions/advanced_tables.c",
                "src/extensions/html_markdown.c",
                "src/extensions/fenced_divs.c",
                "src/extensions/table_html_postprocess.c",
                "src/extensions/inline_footnotes.c",
                "src/extensions/highlight.c",
                "src/extensions/insert.c",
                "src/extensions/sup_sub.c",
                "src/extensions/header_ids.c",
                "src/extensions/relaxed_tables.c",
                "src/extensions/grid_tables.c",
                "src/extensions/citations.c",
                "src/extensions/index.c",
                "src/extensions/concordance.c",
                "src/extensions/syntax_highlight.c",
                "src/pretty_html.c",
                "src/buffer.c",
                "src/parser.c",
                "src/renderer.c",
                "src/utf8.c",
            ],
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("include"),
                .headerSearchPath("include/apex"),
                .headerSearchPath("vendor/cmark-gfm/src"),
                .headerSearchPath("vendor/cmark-gfm/extensions"),
                .headerSearchPath("vendor/libyaml/include"),
                .define("APEX_HAVE_LIBYAML", to: "1"),  // Always enabled when bundled
            ]
        ),
        // Objective-C wrapper
        .target(
            name: "ApexObjC",
            dependencies: ["ApexC"],
            path: "objc",
            exclude: ["Apex.swift", "ApexPlugins.swift"],
            sources: ["NSString+Apex.m", "NSString+ApexPlugins.m"],
            publicHeadersPath: ".",
            cSettings: [
                .headerSearchPath("../include"),
                .unsafeFlags(["-fmodule-map-file=\(apexHeadersModuleMap)"]),
            ]
        ),
        // Swift wrapper
        .target(
            name: "Apex",
            dependencies: ["ApexObjC", "ApexC"],
            path: "objc",
            exclude: ["NSString+Apex.m", "NSString+Apex.h", "NSString+ApexPlugins.m", "NSString+ApexPlugins.h"],
            sources: ["Apex.swift", "ApexPlugins.swift"],
            publicHeadersPath: ".",
            swiftSettings: [
                .define("APEX_HAVE_LIBYAML"),  // Always enabled when bundled
                .unsafeFlags(["-Xcc", "-fmodule-map-file=\(apexHeadersModuleMap)"]),
            ]
        ),
    ]
)
