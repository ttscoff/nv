/**
 * Regression tests from escaping-repro.md (CommonMark escapes, false ![, U+2033).
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include <stdio.h>
#include <string.h>

/* U+2033 DOUBLE PRIME in UTF-8 */
#define UTF8_DOUBLE_PRIME "\xe2\x80\xb3"

void test_escaping_repro(void) {
    int suite_failures = suite_start();
    print_suite_title("Escaping & Unicode repro (escaping-repro.md)", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    char *html;

    /* 1a: Backslash before [ after ! — not an image (CommonMark) */
    const char *escaped_bang_bracket = "Not an image: !\\[literal bracket after bang]";
    html = apex_markdown_to_html(escaped_bang_bracket, strlen(escaped_bang_bracket), &opts);
    assert_not_contains(html, "<img", "escaped ![ is not an image (no img tag)");
    assert_contains(html, "literal bracket after bang", "escaped case preserves link text as plain text");
    apex_free_string(html);

    /* 1b: Sentence with ! immediately before [ — not a valid image; no runaway parse */
    const char *blog_glitch = "I'm so glad you're all here![ Now that the update is slowing down, I will continue.";
    html = apex_markdown_to_html(blog_glitch, strlen(blog_glitch), &opts);
    assert_not_contains(html, "<img", "false ![ opener (blog case) does not emit img");
    assert_contains(html, "I will continue", "false ![ opener preserves tail of sentence");
    apex_free_string(html);

    /* 2–3: U+2033 must not truncate output vs same line with ASCII \" */
    char height_utf8[512];
    snprintf(height_utf8, sizeof(height_utf8),
             "Height in parentheses: (He's 5'7%s if you're wondering.) More text after the parens.",
             UTF8_DOUBLE_PRIME);
    html = apex_markdown_to_html(height_utf8, strlen(height_utf8), &opts);
    assert_contains(html, "More text after the parens", "U+2033 height line: full sentence preserved");
    assert_contains(html, "wondering", "U+2033 height line: middle of sentence preserved");
    apex_free_string(html);

    const char *height_ascii =
        "Height in parentheses: (He's 5'7\" if you're wondering.) More text after the parens.";
    html = apex_markdown_to_html(height_ascii, strlen(height_ascii), &opts);
    assert_contains(html, "More text after the parens", "ASCII quote height line: full sentence preserved");
    apex_free_string(html);

    char minimal_prime[64];
    snprintf(minimal_prime, sizeof(minimal_prime), "X%s Y", UTF8_DOUBLE_PRIME);
    html = apex_markdown_to_html(minimal_prime, strlen(minimal_prime), &opts);
    assert_contains(html, ">X", "minimal U+2033 line: leading X preserved");
    assert_contains(html, " Y", "minimal U+2033 line: space+Y preserved");
    apex_free_string(html);

    html = apex_markdown_to_html("X\" Y", 5, &opts);
    assert_contains(html, " Y", "minimal ASCII quote line: space+Y preserved");
    apex_free_string(html);

    /* 4: True image still parses */
    const char *real_img = "![alt text](https://example.com/image.png)";
    html = apex_markdown_to_html(real_img, strlen(real_img), &opts);
    assert_contains(html, "<img", "real ![alt](url) still produces img");
    assert_contains(html, "example.com/image.png", "real image preserves src");
    apex_free_string(html);

    /* 5: Glued ** after paragraph (stress) */
    char glued_buf[512];
    snprintf(glued_buf, sizeof(glued_buf),
             "End of paragraph with a closing paren. (He's 5'7%s if you're wondering.) Hope this "
             "helps.**Next section** starts here.",
             UTF8_DOUBLE_PRIME);
    html = apex_markdown_to_html(glued_buf, strlen(glued_buf), &opts);
    assert_contains(html, "Hope this helps", "glued bold: text before ** preserved");
    assert_contains(html, "<strong>Next section</strong>", "glued bold: **Next section** is strong");
    assert_contains(html, "starts here", "glued bold: tail preserved");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Escaping & Unicode repro (escaping-repro.md)", had_failures, false);
}
