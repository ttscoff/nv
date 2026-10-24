/**
 * Test Helper Functions Implementation
 */

#include "test_helpers.h"
#include <string.h>
#include <stdarg.h>

/* Test statistics (shared across all test files) */
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

/* When non-zero, only failing tests (and their context) are printed */
int errors_only_output = 0;

/* When non-zero, suppress all output except final count */
int badge_mode = 0;

/**
 * Assert that string contains substring
 */
bool assert_contains(const char *haystack, const char *needle, const char *test_name) {
    tests_run++;

    if (strstr(haystack, needle) != NULL) {
        tests_passed++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", test_name);
        }
        return true;
    } else {
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " %s\n", test_name);
        printf("  Looking for: %s\n", needle);
        printf("  In:          %s\n", haystack);
        return false;
    }
}

/**
 * Assert that string does NOT contain substring
 */
bool assert_not_contains(const char *haystack, const char *needle, const char *test_name) {
    tests_run++;

    if (strstr(haystack, needle) == NULL) {
        tests_passed++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", test_name);
        }
        return true;
    } else {
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " %s\n", test_name);
        printf("  Should NOT contain: %s\n", needle);
        printf("  But found in:        %s\n", haystack);
        return false;
    }
}

/**
 * Assert that a boolean option is set correctly
 */
bool assert_option_bool(bool actual, bool expected, const char *test_name) {
    tests_run++;
    if (actual == expected) {
        tests_passed++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", test_name);
        }
        return true;
    } else {
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " %s\n", test_name);
        printf("  Expected: %s, Got: %s\n", expected ? "true" : "false", actual ? "true" : "false");
        return false;
    }
}

/**
 * Assert that a string option matches
 */
bool assert_option_string(const char *actual, const char *expected, const char *test_name) {
    tests_run++;
    if (actual && expected && strcmp(actual, expected) == 0) {
        tests_passed++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", test_name);
        }
        return true;
    } else {
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " %s\n", test_name);
        printf("  Expected: %s, Got: %s\n", expected ? expected : "(null)", actual ? actual : "(null)");
        return false;
    }
}

/**
 * Report a test result (for manual test cases)
 * Updates test statistics and prints output based on errors_only_output flag
 */
void test_result(bool passed, const char *test_name) {
    tests_run++;
    if (passed) {
        tests_passed++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", test_name);
        }
    } else {
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " %s\n", test_name);
    }
}

/**
 * Report a test result with formatted message (for manual test cases)
 * Updates test statistics and prints output based on errors_only_output flag
 */
void test_resultf(bool passed, const char *format, ...) {
    tests_run++;
    if (passed) {
        tests_passed++;
        if (!errors_only_output) {
            va_list args;
            va_start(args, format);
            printf(COLOR_GREEN "✓" COLOR_RESET " ");
            vprintf(format, args);
            printf("\n");
            va_end(args);
        }
    } else {
        tests_failed++;
        va_list args;
        va_start(args, format);
        printf(COLOR_RED "✗" COLOR_RESET " ");
        vprintf(format, args);
        printf("\n");
        va_end(args);
    }
}

/**
 * Start tracking a test suite
 * Returns the current failure count to track if this suite has failures
 */
int suite_start(void) {
    return tests_failed;
}

/**
 * End tracking a test suite and check if it had failures
 */
bool suite_end(int suite_start_failures) {
    return tests_failed > suite_start_failures;
}

/**
 * Print suite title conditionally based on mode
 * @param title the suite title to print
 * @param suite_had_failures true if the suite had any failures (only used in errors-only mode)
 * @param at_start true if called at start of suite, false if at end
 */
void print_suite_title(const char *title, bool suite_had_failures, bool at_start) {
    /* Never print in badge mode */
    if (badge_mode) {
        return;
    }
    
    /* In errors-only mode: only print at end if suite had failures */
    if (errors_only_output) {
        if (at_start) {
            return; /* Don't print at start in errors-only mode */
        }
        if (!suite_had_failures) {
            return; /* Don't print at end if no failures */
        }
    }
    
    /* Print in normal mode (at start), or in errors-only mode (at end if failures) */
    printf("\n=== %s ===\n", title);
}
