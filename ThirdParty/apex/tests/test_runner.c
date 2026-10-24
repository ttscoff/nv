/**
 * Apex Test Runner
 * Main test runner that calls all test suites
 */

#include "test_helpers.h"
#include "apex/apex.h"

#include <string.h>
#include <stddef.h>
#include <signal.h>
#include <stdlib.h>

/* Track current test name for error reporting */
static const char *current_test_name = NULL;

/* Signal handler for assertion failures */
static void sigabrt_handler(int sig) {
    (void)sig;
    if (current_test_name) {
        fprintf(stderr, "\n*** Assertion failure in test: %s ***\n", current_test_name);
    }
    fprintf(stderr, "This is likely a cmark block finalization issue.\n");
    fprintf(stderr, "Set APEX_SKIP_PROBLEMATIC_TESTS=1 to skip tests that trigger this.\n");
}

/* Forward declarations for individual test suites */
void test_basic_markdown(void);
void test_gfm_features(void);
void test_metadata(void);
void test_metadata_yaml_emit(void);
void test_metadata_yaml_fallback(void);
void test_mmd_metadata_keys(void);
void test_metadata_transforms(void);
void test_metadata_control_options(void);
void test_syntax_highlight_options(void);
void test_syntax_highlight_integration(void);
void test_wiki_links(void);
void test_image_embedding(void);
void test_image_width_height_conversion(void);
void test_math(void);
void test_critic_markup(void);
void test_cmark_init_callback(void);
void test_cmark_callback(void);
void test_custom_plugins(void);
void test_processor_modes(void);
void test_quarto_mode(void);
void test_multimarkdown_image_attributes(void);
void test_reference_definitions(void);
void test_file_includes(void);
void test_ial(void);
void test_bear_image_attributes(void);
void test_bracketed_spans(void);
void test_definition_lists(void);
void test_advanced_tables(void);
void test_relaxed_tables(void);
void test_comprehensive_table_features(void);
void test_table_no_trailing_newline(void);
void test_table_cr_line_endings(void);
void test_combine_gitbook_like(void);
void test_callouts(void);
void test_blockquote_lists(void);
void test_toc(void);
void test_terminal_output(void);
void test_rtf_output(void);
void test_output_formats(void);
void test_html_markdown_attributes(void);
void test_fenced_divs(void);
void test_sup_sub(void);
void test_mixed_lists(void);
void test_unsafe_mode(void);
void test_abbreviations(void);
void test_mmd6_features(void);
void test_emoji(void);
void test_special_markers(void);
void test_inline_tables(void);
void test_grid_tables(void);
void test_insert_syntax(void);
void test_advanced_footnotes(void);
void test_standalone_output(void);
void test_pretty_html(void);
void test_xhtml_output(void);
void test_header_ids(void);
void test_image_captions(void);
void test_indices(void);
void test_citations(void);
void test_aria_labels(void);
void test_marked_integration_features(void);
void test_plugins_integration(void);
void test_ast_json_parser(void);
void test_escaping_repro(void);

/**
 * Test suite registry
 *
 * Maps human-friendly suite names (command-line arguments) to the
 * corresponding test functions.
 */
typedef void (*test_fn)(void);

typedef struct {
    const char *name;
    test_fn fn;
} test_suite;

static test_suite suites[] = {
    { "tests_basic",                   test_basic_markdown },
    { "basic",                         test_basic_markdown },
    { "gfm",                           test_gfm_features },
    { "metadata",                      test_metadata },
    { "metadata_yaml_emit",            test_metadata_yaml_emit },
    { "metadata_yaml_fallback",        test_metadata_yaml_fallback },
    { "metadata_transforms",           test_metadata_transforms },
    { "mmd_metadata_keys",             test_mmd_metadata_keys },
    { "metadata_control_options",      test_metadata_control_options },
    { "syntax_highlight_options",      test_syntax_highlight_options },
    { "syntax_highlight_integration",  test_syntax_highlight_integration },
    { "wiki_links",                    test_wiki_links },
    { "math",                          test_math },
    { "critic_markup",                 test_critic_markup },
    { "cmark_init_callback",           test_cmark_init_callback },
    { "cmark_callback",                test_cmark_callback },
    { "multimarkdown_image_attributes",test_multimarkdown_image_attributes },
    { "reference_definitions",         test_reference_definitions },
    { "processor_modes",               test_processor_modes },
    { "quarto_mode",                   test_quarto_mode },
    { "file_includes",                 test_file_includes },
    { "ial",                           test_ial },
    { "bear_image_attributes",         test_bear_image_attributes },
    { "bracketed_spans",               test_bracketed_spans },
    { "definition_lists",              test_definition_lists },
    { "advanced_tables",               test_advanced_tables },
    { "relaxed_tables",                test_relaxed_tables },
    { "comprehensive_table_features",  test_comprehensive_table_features },
    { "table_no_trailing_newline",     test_table_no_trailing_newline },
    { "table_cr_line_endings",         test_table_cr_line_endings },
    { "combine_gitbook_like",          test_combine_gitbook_like },
    { "callouts",                      test_callouts },
    { "blockquote_lists",              test_blockquote_lists },
    { "toc",                           test_toc },
    { "terminal_output",               test_terminal_output },
    { "rtf_output",                    test_rtf_output },
    { "output_formats",                test_output_formats },
    { "html_markdown_attributes",      test_html_markdown_attributes },
    { "fenced_divs",                   test_fenced_divs },
    { "sup_sub",                       test_sup_sub },
    { "mixed_lists",                   test_mixed_lists },
    { "unsafe_mode",                   test_unsafe_mode },
    { "abbreviations",                 test_abbreviations },
    { "mmd6_features",                 test_mmd6_features },
    { "emoji",                         test_emoji },
    { "special_markers",               test_special_markers },
    { "inline_tables",                 test_inline_tables },
    { "grid_tables",                   test_grid_tables },
    { "insert_syntax",                 test_insert_syntax },
    { "advanced_footnotes",            test_advanced_footnotes },
    { "standalone_output",             test_standalone_output },
    { "pretty_html",                   test_pretty_html },
    { "xhtml_output",                  test_xhtml_output },
    { "header_ids",                    test_header_ids },
    { "image_captions",                test_image_captions },
    { "image_embedding",               test_image_embedding },
    { "image_width_height_conversion", test_image_width_height_conversion },
    { "indices",                       test_indices },
    { "citations",                     test_citations },
    { "aria_labels",                   test_aria_labels },
    { "marked_integration",            test_marked_integration_features },
    { "plugins_integration",           test_plugins_integration },
    { "plugins_custom",                test_custom_plugins },
    { "ast_json",                      test_ast_json_parser },
    { "escaping_repro",                test_escaping_repro },
    { "escaping",                      test_escaping_repro },
};

static const size_t suite_count = sizeof(suites) / sizeof(suites[0]);

/**
 * Main test runner
 */
int main(int argc, char *argv[]) {
    /* Install signal handler for better error reporting */
    signal(SIGABRT, sigabrt_handler);

    /* Parse command-line arguments */
    int errors_only = 0;
    int badge_flag = 0;
    const char *requested_suite = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--errors-only") == 0 || strcmp(argv[i], "-e") == 0) {
            errors_only = 1;
        } else if (strcmp(argv[i], "--badge") == 0 || strcmp(argv[i], "-b") == 0) {
            badge_flag = 1;
        } else if (!requested_suite) {
            requested_suite = argv[i];
        }
    }

    /* Set global badge_mode flag in test helpers */
    if (badge_flag) {
        badge_mode = 1;
    }

    /* Skip header in badge mode */
    if (!badge_flag) {
        printf("Apex Test Suite v%s\n", apex_version_string());
        printf("==========================================\n");
    }

    /* Propagate errors-only mode to test helpers */
    if (errors_only) {
        errors_only_output = 1;
    }

    /* In badge mode, suppress all output except the final count */
    if (badge_flag) {
        errors_only_output = 1; /* Suppress test output */
    }

    if (requested_suite) {
        // List available test suites
        if (strcmp(requested_suite, "list") == 0 || strcmp(requested_suite, "--list") == 0) {
            printf("Available test suites:\n");
            for (size_t i = 0; i < suite_count; i++) {
                printf("  %s\n", suites[i].name);
            }
            return 0;
        }

        // Run only the specified test suite
        for (size_t i = 0; i < suite_count; i++) {
            if (strcmp(requested_suite, suites[i].name) == 0) {
                current_test_name = suites[i].name;
                suites[i].fn();
                current_test_name = NULL;
                goto done_single_suite;
            }
        }

        if (!badge_mode) {
            printf("Unknown test suite: %s\n", requested_suite);
        }
        return 2;
    } else {
        // Run all test suites
        for (size_t i = 0; i < suite_count; i++) {
            current_test_name = suites[i].name;
            suites[i].fn();
            current_test_name = NULL;
        }
    }

done_single_suite:

    /* In badge mode, output only the count */
    if (badge_flag) {
        printf("%d/%d\n", tests_passed, tests_run);
        return (tests_failed == 0) ? 0 : 1;
    }

    /* Print results */
    printf("\n==========================================\n");
    printf("Results: %d total, ", tests_run);
    printf(COLOR_GREEN "%d passed" COLOR_RESET ", ", tests_passed);
    printf(COLOR_RED "%d failed" COLOR_RESET "\n", tests_failed);

    if (tests_failed == 0) {
        printf(COLOR_GREEN "\nAll tests passed! ✓" COLOR_RESET "\n");
        return 0;
    } else {
        printf(COLOR_RED "\nSome tests failed!" COLOR_RESET "\n");
        return 1;
    }
}
