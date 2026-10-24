/**
 * Basic Tests
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include <string.h>

void test_basic_markdown(void) {
    int suite_failures = suite_start();
    print_suite_title("Basic Markdown Tests", false, true);

    apex_options opts = apex_options_default();
    char *html;

    /* Test headers */
    html = apex_markdown_to_html("# Header 1", 10, &opts);
    assert_contains(html, "<h1", "H1 header tag");
    assert_contains(html, "Header 1</h1>", "H1 header content");
    assert_contains(html, "id=", "H1 header has ID");
    apex_free_string(html);

    /* Test emphasis */
    html = apex_markdown_to_html("**bold** and *italic*", 21, &opts);
    assert_contains(html, "<strong>bold</strong>", "Bold text");
    assert_contains(html, "<em>italic</em>", "Italic text");
    apex_free_string(html);

    /* Test lists */
    html = apex_markdown_to_html("- Item 1\n- Item 2", 17, &opts);
    assert_contains(html, "<ul>", "Unordered list");
    assert_contains(html, "<li>Item 1</li>", "List item");
    apex_free_string(html);
    
    bool had_failures = suite_end(suite_failures);
    print_suite_title("Basic Markdown Tests", had_failures, false);
}

/**
 * Test GFM features
 */

void test_gfm_features(void) {
    int suite_failures = suite_start();
    print_suite_title("GFM Features Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_GFM);
    char *html;

    /* Test strikethrough */
    html = apex_markdown_to_html("~~deleted~~", 11, &opts);
    assert_contains(html, "<del>deleted</del>", "Strikethrough");
    apex_free_string(html);

    /* Test task lists */
    html = apex_markdown_to_html("- [ ] Todo\n- [x] Done", 22, &opts);
    assert_contains(html, "checkbox", "Task list checkbox");
    apex_free_string(html);

    /* Test tables */
    const char *table = "| H1 | H2 |\n|-----|-----|\n| C1 | C2 |";
    html = apex_markdown_to_html(table, strlen(table), &opts);
    assert_contains(html, "<table>", "GFM table");
    assert_contains(html, "<th>H1</th>", "Table header");
    assert_contains(html, "<td>C1</td>", "Table cell");
    apex_free_string(html);
    
    bool had_failures = suite_end(suite_failures);
    print_suite_title("GFM Features Tests", had_failures, false);
}

/**
 * Test metadata
 */
