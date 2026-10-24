/**
 * Syntax Highlighting Integration Tests
 *
 * These tests exercise the actual external highlighters (pygments/skylighting)
 * via the core pipeline (apex_markdown_to_html), plus a couple of targeted
 * edge cases (tool missing, language-only mode).
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include "../src/extensions/syntax_highlight.h"

#include <string.h>
#include <stdlib.h>

static void with_env(const char *key, const char *value, void (*fn)(void *), void *ctx) {
    const char *old = getenv(key);
    char *old_dup = old ? strdup(old) : NULL;

    if (value) {
        setenv(key, value, 1);
    } else {
        unsetenv(key);
    }

    fn(ctx);

    if (old_dup) {
        setenv(key, old_dup, 1);
        free(old_dup);
    } else {
        unsetenv(key);
    }
}

static void test_tool_missing_cb(void *ctx) {
    (void)ctx;
    apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    opts.code_highlighter = "pygments";

    const char *md =
        "```python\n"
        "print(\"hi\")\n"
        "```\n";
    char *html = apex_markdown_to_html(md, strlen(md), &opts);

    /* When PATH is empty, tool check should fail and we should fall back to raw <pre><code>. */
    assert_contains(html, "<pre", "missing tool: keeps <pre>");
    assert_contains(html, "<code", "missing tool: keeps <code>");
    assert_not_contains(html, "class=\"highlight\"", "missing tool: no pygments wrapper");

    apex_free_string(html);
}

static void test_tool_missing_with_suppression(void *ctx) {
    /* Set suppression variable, then clear PATH and run the test */
    with_env("PATH", "", test_tool_missing_cb, ctx);
}

static int contains_in_order(const char *haystack, const char *a, const char *b) {
    const char *pa = strstr(haystack, a);
    const char *pb = strstr(haystack, b);
    return (pa && pb && pa < pb) ? 1 : 0;
}

void test_syntax_highlight_integration(void) {
    int suite_failures = suite_start();
    print_suite_title("Syntax Highlighting Integration Tests", false, true);

    /* Exercise tool-missing branch by clearing PATH during conversion. */
    /* Suppress the warning for this intentional test case */
    with_env("APEX_SUPPRESS_HIGHLIGHT_WARNINGS", "1", test_tool_missing_with_suppression, NULL);

    /* Pygments: basic highlight with language */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.code_highlighter = "pygments";

        const char *md =
            "```python\n"
            "print(\"hi\")\n"
            "```\n";
        char *html = apex_markdown_to_html(md, strlen(md), &opts);
        assert_contains(html, "class=\"highlight\"", "pygments: emits highlight wrapper");
        assert_contains(html, "print", "pygments: preserves code content");
        apex_free_string(html);
    }

    /* Pygments: line numbers */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.code_highlighter = "pygments";
        opts.code_line_numbers = true;

        const char *md =
            "```python\n"
            "print(\"hi\")\n"
            "```\n";
        char *html = apex_markdown_to_html(md, strlen(md), &opts);
        assert_contains(html, "class=\"linenos\"", "pygments: line numbers include linenos");
        apex_free_string(html);
    }

    /* Pygments: language-only mode should skip unlanguaged blocks */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.code_highlighter = "pygments";
        opts.highlight_language_only = true;

        const char *md =
            "```\n"
            "print(\"no lang\")\n"
            "```\n";
        char *html = apex_markdown_to_html(md, strlen(md), &opts);
        assert_contains(html, "<pre", "language-only: still has code block");
        assert_not_contains(html, "class=\"highlight\"", "language-only: unlanguaged block not highlighted");
        apex_free_string(html);
    }

    /* Pygments: ensure unescape_html path is exercised (entities in code) */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.code_highlighter = "pygments";

        const char *md =
            "```python\n"
            "print(\"<tag>&\")\n"
            "```\n";
        char *html = apex_markdown_to_html(md, strlen(md), &opts);
        assert_contains(html, "class=\"highlight\"", "pygments: highlighted wrapper present (entities case)");
        assert_contains(html, "&lt;tag&gt;", "pygments: highlighted output contains escaped <tag>");
        assert_contains(html, "&amp;", "pygments: highlighted output contains escaped &");
        apex_free_string(html);
    }

    /* Skylighting: basic highlight */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.code_highlighter = "skylighting";

        const char *md =
            "```python\n"
            "print(\"hi\")\n"
            "```\n";
        char *html = apex_markdown_to_html(md, strlen(md), &opts);
        assert_contains(html, "class=\"sourceCode\"", "skylighting: emits sourceCode wrapper");
        assert_contains(html, "print", "skylighting: preserves code content");
        apex_free_string(html);
    }

    /* Skylighting: line numbers */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.code_highlighter = "skylighting";
        opts.code_line_numbers = true;

        const char *md =
            "```python\n"
            "print(\"hi\")\n"
            "```\n";
        char *html = apex_markdown_to_html(md, strlen(md), &opts);
        assert_contains(html, "numberSource", "skylighting: line numbers add numberSource class");
        apex_free_string(html);
    }

    /* Skylighting: language-only mode skips unlanguaged blocks (and still highlights languaged ones) */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.code_highlighter = "skylighting";
        opts.highlight_language_only = true;

        const char *md =
            "```python\n"
            "print(\"yes\")\n"
            "```\n"
            "\n"
            "```\n"
            "print(\"no\")\n"
            "```\n";
        char *html = apex_markdown_to_html(md, strlen(md), &opts);
        test_result(contains_in_order(html, "class=\"sourceCode\"", "<pre") == 1,
                    "skylighting: first block highlighted, later raw <pre> remains");
        apex_free_string(html);
    }

    /* Direct function call: cover code-tag class=\"language-...\" extraction path */
    {
        const char *html_in =
            "<pre><code class=\"language-python\">print(&quot;hi&quot;)\n</code></pre>";
        char *out = apex_apply_syntax_highlighting(html_in, "pygments", false, false, false, NULL);
        assert_contains(out, "class=\"highlight\"", "direct: class=language-... triggers highlighting");
        free(out);
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Syntax Highlighting Integration Tests", had_failures, false);
}

