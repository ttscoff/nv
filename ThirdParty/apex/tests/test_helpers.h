/**
 * Test Helper Functions
 * Shared test infrastructure for Apex test suite
 */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdbool.h>
#include <stdio.h>

/* Test statistics (shared across all test files) */
extern int tests_run;
extern int tests_passed;
extern int tests_failed;

/* When non-zero, only failing tests (and their context) are printed */
extern int errors_only_output;

/* When non-zero, suppress all output except final count */
extern int badge_mode;

/* Color codes for terminal output */
#define COLOR_GREEN "\033[0;32m"
#define COLOR_RED "\033[0;31m"
#define COLOR_RESET "\033[0m"

/**
 * Assert that string contains substring
 */
bool assert_contains(const char *haystack, const char *needle, const char *test_name);

/**
 * Assert that string does NOT contain substring
 */
bool assert_not_contains(const char *haystack, const char *needle, const char *test_name);

/**
 * Assert that a boolean option is set correctly
 */
bool assert_option_bool(bool actual, bool expected, const char *test_name);

/**
 * Assert that a string option matches
 */
bool assert_option_string(const char *actual, const char *expected, const char *test_name);

/**
 * Report a test result (for manual test cases)
 * Updates test statistics and prints output based on errors_only_output flag
 * @param passed true if test passed, false if failed
 * @param test_name the name/description of the test
 */
void test_result(bool passed, const char *test_name);

/**
 * Report a test result with formatted message (for manual test cases)
 * Updates test statistics and prints output based on errors_only_output flag
 * @param passed true if test passed, false if failed
 * @param format printf-style format string
 * @param ... arguments for format string
 */
void test_resultf(bool passed, const char *format, ...);

/**
 * Start tracking a test suite
 * Saves current test counts to track suite-level pass/fail
 * @return suite ID (current tests_failed count)
 */
int suite_start(void);

/**
 * End tracking a test suite and check if it had failures
 * @param suite_start_failures the tests_failed count when suite started
 * @return true if suite had any failures, false if all passed
 */
bool suite_end(int suite_start_failures);

/**
 * Print suite title conditionally based on mode
 * - Always prints in normal mode (at start)
 * - Never prints in badge mode
 * - In errors-only mode: only prints at end if suite had failures
 * @param title the suite title to print
 * @param suite_had_failures true if the suite had any failures (only used in errors-only mode at end)
 * @param at_start true if called at start of suite, false if at end
 */
void print_suite_title(const char *title, bool suite_had_failures, bool at_start);

#endif /* TEST_HELPERS_H */

