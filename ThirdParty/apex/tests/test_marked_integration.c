/**
 * Marked Integration Features Tests
 * Tests for features added for Marked integration
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include <string.h>
#include <stdlib.h>

void test_marked_integration_features(void) {
    int suite_failures = suite_start();
    print_suite_title("Marked Integration Features Tests", false, true);

    apex_options opts = apex_options_default();
    char *html;

    /* Test widont feature */
    opts.enable_widont = true;
    html = apex_markdown_to_html("# woe is me", 12, &opts);
    assert_contains(html, "woe&nbsp;is&nbsp;me", "Widont applies to short trailing words");
    apex_free_string(html);

    html = apex_markdown_to_html("# hello world", 13, &opts);
    assert_contains(html, "hello&nbsp;world", "Widont applies to 10-character trailing words");
    apex_free_string(html);

    html = apex_markdown_to_html("# introduction to the topic", 28, &opts);
    /* Should apply widont to trailing \"to the topic\" (12 chars) to prevent short widow */
    if (strstr(html, "introduction to&nbsp;the&nbsp;topic") != NULL) {
        test_result(true, "Widont applies to trailing words in long headings, including enough to exceed 10 chars");
    } else {
        test_result(false, "Widont did not apply to trailing words in long heading");
    }
    apex_free_string(html);

    /* Test code-is-poetry feature */
    opts = apex_options_default();
    opts.code_is_poetry = true;
    opts.highlight_language_only = true;  /* Code-is-poetry implies this */
    html = apex_markdown_to_html("```\nplain code block\n```", 25, &opts);
    assert_contains(html, "class=\"poetry\"", "Code-is-poetry adds poetry class to unlanguaged blocks");
    assert_contains(html, "<pre><code class=\"poetry\">", "Poetry class on code element");
    apex_free_string(html);

    /* Test that language blocks don't get poetry class */
    html = apex_markdown_to_html("```python\ndef hello():\n    pass\n```", 35, &opts);
    assert_not_contains(html, "class=\"poetry\"", "Language blocks don't get poetry class");
    /* cmark-gfm may use lang attribute on pre or class on code - check for either */
    if (strstr(html, "class=\"language-python\"") || strstr(html, "lang=\"python\"")) {
        test_result(true, "Language blocks keep language class or lang attribute");
    } else {
        test_result(false, "Language blocks should have language class or lang attribute");
    }
    apex_free_string(html);

    /* Test indented code blocks with poetry */
    html = apex_markdown_to_html("    plain indented code", 24, &opts);
    assert_contains(html, "class=\"poetry\"", "Poetry class applies to indented code blocks");
    apex_free_string(html);

    /* Test markdown-in-html toggle */
    opts = apex_options_default();
    opts.enable_markdown_in_html = true;
    html = apex_markdown_to_html("<div markdown=\"1\">**bold**</div>", 34, &opts);
    assert_contains(html, "<strong>bold</strong>", "Markdown-in-html processes markdown inside HTML");
    apex_free_string(html);

    opts.enable_markdown_in_html = false;
    html = apex_markdown_to_html("<div markdown=\"1\">**bold**</div>", 34, &opts);
    assert_contains(html, "**bold**", "Markdown-in-html disabled does not process markdown");
    apex_free_string(html);

    /* Test random footnote IDs */
    opts = apex_options_default();
    opts.enable_footnotes = true;
    opts.random_footnote_ids = true;
    html = apex_markdown_to_html("Text[^1]\n\n[^1]: Footnote", 25, &opts);
    /* Should have hash prefix in IDs - format is fn-XXXXXXXX-1 where X is 8-char hash */
    const char *fn_id = strstr(html, "id=\"fn-");
    const char *fnref_id = strstr(html, "id=\"fnref-");
    if (fn_id && fnref_id) {
        /* Find where the ID value starts (after "id=\"fn-" or "id=\"fnref-") */
        const char *fn_value_start = fn_id + 7;  /* After "id=\"fn-" */
        const char *fnref_value_start = fnref_id + 10;  /* After "id=\"fnref-" */

        /* Find the closing quote to get the full ID value */
        const char *fn_quote = strchr(fn_value_start, '"');
        const char *fnref_quote = strchr(fnref_value_start, '"');

        if (fn_quote && fnref_quote) {
            size_t fn_id_len = fn_quote - fn_value_start;
            size_t fnref_id_len = fnref_quote - fnref_value_start;

            /* IDs should be longer than "1" (should be "XXXXXXXX-1" format) */
            /* Check that there's at least 9 characters (8-char hash + dash + number) */
            if (fn_id_len > 9 && fnref_id_len > 9) {
                /* Check that it contains hex characters (hash) */
                bool fn_has_hex = false;
                bool fnref_has_hex = false;
                for (size_t i = 0; i < fn_id_len && i < 8; i++) {
                    char c = fn_value_start[i];
                    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                        fn_has_hex = true;
                        break;
                    }
                }
                for (size_t i = 0; i < fnref_id_len && i < 8; i++) {
                    char c = fnref_value_start[i];
                    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                        fnref_has_hex = true;
                        break;
                    }
                }

                if (fn_has_hex && fnref_has_hex) {
                    test_result(true, "Random footnote IDs include hash prefix");
                } else {
                    test_result(false, "Random footnote IDs hash format incorrect");
                }
            } else {
                test_result(false, "Random footnote IDs too short (missing hash)");
            }
        } else {
            test_result(false, "Random footnote IDs malformed");
        }
    } else {
        test_result(false, "Random footnote IDs not found");
    }
    apex_free_string(html);

    /* Test hashtags - comprehensive tests */
    opts = apex_options_default();
    opts.enable_hashtags = true;

    /* Test hashtags within text (not at beginning of line) */
    html = apex_markdown_to_html("This is a #hashtag in text.", 30, &opts);
    assert_contains(html, "<span class=\"mkhashtag\">#hashtag</span>", "Hashtags within text are wrapped in spans");
    apex_free_string(html);

    /* Test hashtags at beginning of line (no space before alphanumeric) */
    html = apex_markdown_to_html("#hashtag at start of line", 26, &opts);
    assert_contains(html, "<span class=\"mkhashtag\">#hashtag</span>", "Hashtags at beginning of line are converted");
    apex_free_string(html);

    /* Test hashtags with subtags */
    html = apex_markdown_to_html("This is a #tag/subtag in text.", 33, &opts);
    assert_contains(html, "<span class=\"mkhashtag\">#tag/subtag</span>", "Hashtags with subtags are recognized");
    apex_free_string(html);

    /* Test that headlines (# with space) are NOT converted */
    html = apex_markdown_to_html("# This is a headline", 22, &opts);
    /* Headlines may have ID attributes, so check for h1 content */
    assert_contains(html, "<h1", "Headlines with space create h1 tag");
    assert_contains(html, "This is a headline", "Headlines with space contain headline text");
    assert_not_contains(html, "<span class=\"mkhashtag\">#", "Headlines are not treated as hashtags");
    apex_free_string(html);

    /* Test multiple hashtags in one line */
    html = apex_markdown_to_html("This has #tag1 and #tag2 in it.", 35, &opts);
    assert_contains(html, "<span class=\"mkhashtag\">#tag1</span>", "First hashtag in multiple hashtags line");
    assert_contains(html, "<span class=\"mkhashtag\">#tag2</span>", "Second hashtag in multiple hashtags line");
    apex_free_string(html);

    /* Test hashtags in code blocks are not processed */
    html = apex_markdown_to_html("```\n#hashtag in code\n```", 26, &opts);
    assert_not_contains(html, "class=\"mkhashtag\"", "Hashtags in code blocks are not processed");
    apex_free_string(html);

    /* Test hashtags in indented code blocks are not processed */
    html = apex_markdown_to_html("    #hashtag in indented code", 30, &opts);
    assert_not_contains(html, "class=\"mkhashtag\"", "Hashtags in indented code blocks are not processed");
    apex_free_string(html);

    /* Test style-hashtags - basic */
    opts.style_hashtags = true;
    html = apex_markdown_to_html("This is a #hashtag in text.", 30, &opts);
    assert_contains(html, "<span class=\"mkstyledtag\">#hashtag</span>", "Style-hashtags uses mkstyledtag class");
    apex_free_string(html);

    /* Test style-hashtags at beginning of line */
    html = apex_markdown_to_html("#hashtag at start", 17, &opts);
    assert_contains(html, "<span class=\"mkstyledtag\">#hashtag</span>", "Style-hashtags at beginning of line");
    apex_free_string(html);

    /* Test style-hashtags with subtags */
    html = apex_markdown_to_html("This is a #tag/subtag in text.", 33, &opts);
    assert_contains(html, "<span class=\"mkstyledtag\">#tag/subtag</span>", "Style-hashtags with subtags");
    apex_free_string(html);

    /* Test that headlines are still not converted with style-hashtags */
    html = apex_markdown_to_html("# This is a headline", 22, &opts);
    /* Headlines may have ID attributes, so check for h1 content */
    assert_contains(html, "<h1", "Headlines with space create h1 tag even with style-hashtags");
    assert_contains(html, "This is a headline", "Headlines with space contain headline text");
    assert_not_contains(html, "<span class=\"mkstyledtag\">#", "Headlines are not treated as styled hashtags");
    apex_free_string(html);

    /* Test proofreader mode */
    opts = apex_options_default();
    opts.proofreader_mode = true;
    opts.enable_critic_markup = true;  /* Proofreader implies this */
    opts.critic_mode = 2;  /* Markup mode */
    html = apex_markdown_to_html("This is ==highlighted== text.", 29, &opts);
    assert_contains(html, "<mark class=\"critic\">highlighted</mark>", "Proofreader converts == to highlight");
    apex_free_string(html);

    html = apex_markdown_to_html("This is ~~deleted~~ text.", 26, &opts);
    assert_contains(html, "<del class=\"critic\">deleted</del>", "Proofreader converts ~~ to deletion");
    apex_free_string(html);

    /* Test proofreader in code blocks */
    html = apex_markdown_to_html("```\n==code==\n```", 15, &opts);
    assert_not_contains(html, "<mark class=\"critic\">", "Proofreader does not process code blocks");
    apex_free_string(html);

    /* Test HR page break */
    opts = apex_options_default();
    opts.hr_page_break = true;
    html = apex_markdown_to_html("First\n\n---\n\nSecond", 20, &opts);
    assert_contains(html, "class=\"mkpagebreak manualbreak\"", "HR page break creates page break div");
    assert_contains(html, "title=\"Page break created from HR\"", "HR page break has correct title");
    assert_contains(html, "data-description=\"PAGE (HR)\"", "HR page break has correct description");
    apex_free_string(html);

    /* Test title-from-h1 */
    opts = apex_options_default();
    opts.standalone = true;
    opts.title_from_h1 = true;
    html = apex_markdown_to_html("# My Document Title\n\nContent here.", 40, &opts);
    assert_contains(html, "<title>My Document Title</title>", "Title-from-h1 extracts H1 as title");
    /* H1 may have ID attribute, so check for h1 content */
    assert_contains(html, "<h1", "Title-from-h1 keeps H1 in body");
    assert_contains(html, "My Document Title", "Title-from-h1 H1 contains title text");
    apex_free_string(html);

    /* Test title-from-h1 with explicit title (should not override) */
    opts.document_title = strdup("Explicit Title");
    html = apex_markdown_to_html("# My Document Title\n\nContent here.", 40, &opts);
    assert_contains(html, "<title>Explicit Title</title>", "Title-from-h1 does not override explicit title");
    assert_not_contains(html, "<title>My Document Title</title>", "Explicit title takes precedence");
    free((void*)opts.document_title);
    apex_free_string(html);

    /* Test page-break-before-footnotes */
    opts = apex_options_default();
    opts.enable_footnotes = true;
    opts.page_break_before_footnotes = true;
    html = apex_markdown_to_html("Text[^1]\n\n[^1]: Footnote", 25, &opts);
    assert_contains(html, "class=\"mkpagebreak manualbreak\"", "Page break before footnotes creates page break div");
    assert_contains(html, "title=\"Page break created before footnotes\"", "Page break has correct title");
    assert_contains(html, "data-description=\"PAGE (Footnotes)\"", "Page break has correct description");
    /* Check that it appears before footnotes section */
    const char *page_break = strstr(html, "class=\"mkpagebreak");
    const char *footnotes = strstr(html, "<section class=\"footnotes\"");
    if (page_break && footnotes && page_break < footnotes) {
        test_result(true, "Page break appears before footnotes section");
    } else {
        test_result(false, "Page break should appear before footnotes section");
    }
    apex_free_string(html);

    /* Indented {::pagebreak /} must not break markdown-in-html blockquote processing */
    opts = apex_options_default();
    opts.enable_markdown_in_html = true;
    opts.enable_marked_extensions = true;
    const char *pagebreak_html =
        "<blockquote class=\"tip\" markdown=\"1\">\n"
        "Tip with [link](doc.html) and <span>HTML</span>.\n"
        "</blockquote>\n"
        "## Page breaks\n"
        "    {::pagebreak /}";
    html = apex_markdown_to_html(pagebreak_html, strlen(pagebreak_html), &opts);
    assert_contains(html, "<blockquote", "Blockquote preserved with markdown-in-html");
    assert_contains(html, "<a href=\"doc.html\">link</a>", "Blockquote markdown parsed inside markdown=\"1\"");
    assert_contains(html, "<h2", "Heading after blockquote rendered");
    assert_contains(html, "<pre><code>{::pagebreak /}", "Indented Leanpub pagebreak marker in code block");
    assert_not_contains(html, "class=\"mkpagebreak", "Indented Leanpub pagebreak marker not converted");
    const char *bq_h2 = strstr(html, "<h2");
    const char *bq_pre = strstr(html, "<pre>");
    if (bq_h2 && bq_pre && bq_h2 < bq_pre) {
        test_result(true, "Heading before code block in markdown-in-html pagebreak repro");
    } else {
        test_result(false, "Heading should appear before code block in markdown-in-html pagebreak repro");
    }
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Marked Integration Features Tests", had_failures, false);
}
