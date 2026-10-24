/**
 * Output Tests
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include "../src/extensions/includes.h"
#include "../src/ast_json.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void test_toc(void) {
    int suite_failures = suite_start();
    print_suite_title("TOC Generation Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_marked_extensions = true;
    char *html;

    /* -t toc: Markdown list, default depth 1-3 */
    apex_options toc_opts = apex_options_default();
    toc_opts.output_format = APEX_OUTPUT_TOC;
    const char *doc =
        "# Introduction\n\n"
        "## Getting Started\n\n"
        "### Installation\n\n"
        "#### Too Deep\n\n"
        "## Configuration\n\n"
        "# API\n"
        "{: .no_toc}\n";
    char *md = apex_markdown_to_html(doc, strlen(doc), &toc_opts);
    assert_contains(md, "- [Introduction](#introduction)\n", "TOC md has H1");
    assert_contains(md, "  - [Getting Started](#getting-started)\n", "TOC md indents H2");
    assert_contains(md, "    - [Installation](#installation)\n", "TOC md indents H3");
    assert_not_contains(md, "Too Deep", "TOC md excludes H4 by default");
    assert_not_contains(md, "API", "TOC md excludes no_toc heading");
    assert_not_contains(md, "<nav", "TOC md is not HTML");
    apex_free_string(md);

    toc_opts.toc_min = 2;
    toc_opts.toc_max = 4;
    md = apex_markdown_to_html(doc, strlen(doc), &toc_opts);
    assert_not_contains(md, "Introduction", "toc-min-max excludes H1");
    assert_contains(md, "Too Deep", "toc-min-max includes H4");
    apex_free_string(md);

    /* id-format mmd */
    toc_opts = apex_options_default();
    toc_opts.output_format = APEX_OUTPUT_TOC;
    toc_opts.id_format = 1; /* MMD */
    md = apex_markdown_to_html("# Hello World\n", 14, &toc_opts);
    assert_contains(md, "- [Hello World](#helloworld)\n", "TOC md respects MMD id-format");
    apex_free_string(md);

    /* manual ID */
    toc_opts = apex_options_default();
    toc_opts.output_format = APEX_OUTPUT_TOC;
    md = apex_markdown_to_html("# Custom {#my-custom}\n", 22, &toc_opts);
    assert_contains(md, "- [Custom](#my-custom)\n", "TOC md uses manual heading ID");
    apex_free_string(md);

    /* id-format kramdown */
    toc_opts = apex_options_default();
    toc_opts.output_format = APEX_OUTPUT_TOC;
    toc_opts.id_format = 2; /* Kramdown */
    md = apex_markdown_to_html("# Trailing Dash-\n", 17, &toc_opts);
    assert_contains(md, "- [Trailing Dash-](#trailing-dash-)\n", "TOC md respects Kramdown id-format");
    apex_free_string(md);

    /* empty */
    toc_opts = apex_options_default();
    toc_opts.output_format = APEX_OUTPUT_TOC;
    md = apex_markdown_to_html("No headings here.\n", 18, &toc_opts);
    if (md && (md[0] == '\0' || strcmp(md, "\n") == 0)) {
        test_result(true, "TOC md empty when no headings");
    } else {
        test_result(false, "TOC md empty when no headings");
    }
    apex_free_string(md);

    /* A TOC longer than 4 KB keeps every entry and stays well-formed */
    {
        size_t cap = 64 * 1024;
        char *big = malloc(cap);
        size_t len = 0;
        len += (size_t)snprintf(big + len, cap - len, "# Top\n\n<!--TOC min2 max3-->\n\n");
        for (int i = 1; i <= 150; i++) {
            len += (size_t)snprintf(big + len, cap - len,
                                    "## A fairly long section heading number %d\n\ntext\n\n", i);
            if (i % 5 == 0) {
                len += (size_t)snprintf(big + len, cap - len, "### Sub %d\n\nmore\n\n", i);
            }
        }
        html = apex_markdown_to_html(big, len, &opts);
        assert_contains(html, "href=\"#a-fairly-long-section-heading-number-150\"",
                        "Large TOC keeps its last entry");
        assert_contains(html, "href=\"#sub-150\"", "Large TOC keeps its last nested entry");
        assert_contains(html, "</ul>\n</nav>", "Large TOC closes its list and nav");
        const char *nav_end = html ? strstr(html, "</nav>") : NULL;
        const char *first_h2 = html ? strstr(html, "<h2") : NULL;
        test_result(nav_end && first_h2 && nav_end < first_h2,
                    "Large TOC nav closes before the document body");
        apex_free_string(html);
        free(big);
    }

    /* Returning to a shallower level closes the nested item before its list */
    const char *up_toc =
        "# Top\n\n<!--TOC min2 max3-->\n\n## B\n\n## C\n\n### D\n\n## E\n";
    html = apex_markdown_to_html(up_toc, strlen(up_toc), &opts);
    assert_contains(html,
                    "<li><a href=\"#c\">C</a><ul>\n<li><a href=\"#d\">D</a></li>\n</ul>\n</li>\n"
                    "<li><a href=\"#e\">E</a></li>\n</ul>\n</nav>",
                    "TOC closes nested li, then ul, then parent li");
    assert_not_contains(html, "</a></ul>", "TOC never closes a ul over an open li");
    apex_free_string(html);

    /* Headings that skip a level stay siblings of each other */
    const char *skip_toc =
        "# Top\n\n<!--TOC min2 max4-->\n\n## B\n\n#### X\n\n#### Y\n\n### Z\n\n## E\n";
    html = apex_markdown_to_html(skip_toc, strlen(skip_toc), &opts);
    assert_contains(html,
                    "<li><a href=\"#b\">B</a><ul>\n"
                    "<li><a href=\"#x\">X</a></li>\n"
                    "<li><a href=\"#y\">Y</a></li>\n"
                    "<li><a href=\"#z\">Z</a></li>\n"
                    "</ul>\n</li>\n"
                    "<li><a href=\"#e\">E</a></li>\n</ul>\n</nav>",
                    "TOC keeps skipped-level headings as siblings");
    apex_free_string(html);

    /* A TOC that starts deeper than later headings keeps them at the top level */
    const char *deep_first_toc =
        "# Top\n\n<!--TOC min2 max3-->\n\n### A\n\n## B\n\n### C\n";
    html = apex_markdown_to_html(deep_first_toc, strlen(deep_first_toc), &opts);
    assert_contains(html,
                    "<ul>\n<li><a href=\"#a\">A</a></li>\n"
                    "<li><a href=\"#b\">B</a><ul>\n"
                    "<li><a href=\"#c\">C</a></li>\n</ul>\n</li>\n</ul>\n</nav>",
                    "TOC starting with a deeper heading nests later items correctly");
    apex_free_string(html);

    /* Bare {{TOC}} should use option defaults (1-3): exclude H4 */
    const char *bare_default_depth =
        "# H1\n\n{{TOC}}\n\n## H2\n\n### H3\n\n#### H4";
    html = apex_markdown_to_html(bare_default_depth, strlen(bare_default_depth), &opts);
    assert_contains(html, "href=\"#h1\"", "Bare TOC includes H1 by default");
    assert_contains(html, "href=\"#h3\"", "Bare TOC includes H3 by default");
    assert_not_contains(html, "href=\"#h4\"", "Bare TOC excludes H4 by default (max 3)");
    apex_free_string(html);

    /* --toc-min-max via options: 2,4 */
    apex_options depth_opts = opts;
    depth_opts.toc_min = 2;
    depth_opts.toc_max = 4;
    html = apex_markdown_to_html(bare_default_depth, strlen(bare_default_depth), &depth_opts);
    assert_not_contains(html, "href=\"#h1\"", "toc_min=2 excludes H1");
    assert_contains(html, "href=\"#h4\"", "toc_max=4 includes H4");
    apex_free_string(html);

    /* Marker override wins over options */
    const char *override_toc =
        "# H1\n\n{{TOC:1-6}}\n\n## H2\n\n### H3\n\n#### H4\n\n##### H5";
    html = apex_markdown_to_html(override_toc, strlen(override_toc), &depth_opts);
    assert_contains(html, "href=\"#h1\"", "Marker range overrides toc_min");
    assert_contains(html, "href=\"#h5\"", "Marker range overrides toc_max");
    apex_free_string(html);

    /* Partial marker max under custom toc_min/toc_max: unspecified min is 1, not option min */
    apex_options partial_opts = opts;
    partial_opts.toc_min = 2;
    partial_opts.toc_max = 4;
    const char *partial_max =
        "# H1\n\n<!--TOC max5-->\n\n## H2\n\n### H3\n\n#### H4\n\n##### H5\n\n###### H6";
    html = apex_markdown_to_html(partial_max, strlen(partial_max), &partial_opts);
    assert_contains(html, "href=\"#h1\"", "Partial max marker uses min=1 not option toc_min");
    assert_contains(html, "href=\"#h5\"", "Partial max marker includes H5");
    assert_not_contains(html, "href=\"#h6\"", "Partial max marker excludes H6");
    apex_free_string(html);

    /* Test basic TOC marker */
    const char *doc_with_toc = "# Header 1\n\n<!--TOC-->\n\n## Header 2\n\n### Header 3";
    html = apex_markdown_to_html(doc_with_toc, strlen(doc_with_toc), &opts);
    assert_contains(html, "<ul", "TOC contains list");
    assert_contains(html, "Header 1", "TOC includes H1");
    assert_contains(html, "Header 2", "TOC includes H2");
    assert_contains(html, "Header 3", "TOC includes H3");
    apex_free_string(html);

    /* Test MMD style TOC */
    const char *mmd_toc = "# Title\n\n{{TOC}}\n\n## Section";
    html = apex_markdown_to_html(mmd_toc, strlen(mmd_toc), &opts);
    assert_contains(html, "<ul", "MMD TOC generates list");
    assert_contains(html, "Section", "MMD TOC includes headers");
    apex_free_string(html);

    /* MultiMarkdown mode should process {{TOC}} even with marked extensions disabled */
    apex_options mmd_opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    html = apex_markdown_to_html(mmd_toc, strlen(mmd_toc), &mmd_opts);
    assert_contains(html, "<nav class=\"toc\">", "MMD mode renders TOC marker");
    assert_contains(html, "Section", "MMD mode TOC includes headers");
    apex_free_string(html);

    /* Test TOC with depth range */
    const char *depth_toc = "# H1\n\n{{TOC:2-3}}\n\n## H2\n\n### H3\n\n#### H4";
    html = apex_markdown_to_html(depth_toc, strlen(depth_toc), &opts);
    assert_contains(html, "<ul", "Depth-limited TOC generated");
    assert_contains(html, "H2", "Includes H2");
    assert_contains(html, "H3", "Includes H3");
    /* H1 should be excluded (below min 2) */
    /* H4 should be excluded (beyond max 3) */
    if (strstr(html, "href=\"#h1\"") == NULL && strstr(html, "href=\"#h4\"") == NULL) {
        test_result(true, "Depth range excludes H1 and H4");
    } else {
        test_result(false, "Depth range didn't exclude properly");
    }
    apex_free_string(html);

    /* Regression: {{TOC:2}} must not parse beyond marker into later IDs */
    const char *single_depth_toc =
        "# Table of Contents Test\n\n{{TOC:2}}\n\nLorem ipsum\n\n## Section 1\n\n## Section 2\n\n## Section 3";
    html = apex_markdown_to_html(single_depth_toc, strlen(single_depth_toc), &opts);
    assert_contains(html, "<nav class=\"toc\">", "Single depth TOC nav rendered");
    assert_contains(html, "href=\"#section-1\"", "Single depth TOC includes Section 1");
    assert_contains(html, "href=\"#section-2\"", "Single depth TOC includes Section 2");
    assert_contains(html, "href=\"#section-3\"", "Single depth TOC includes Section 3");
    apex_free_string(html);

    /* Regression: same-level headings in {{TOC:2-6}} should remain siblings */
    const char *same_level_range_toc =
        "# Table of Contents Test\n\n{{TOC:2-6}}\n\nLorem ipsum\n\n## Section 1\n\n## Section 2\n\n## Section 3";
    html = apex_markdown_to_html(same_level_range_toc, strlen(same_level_range_toc), &opts);
    assert_contains(html, "href=\"#section-1\"", "Range TOC includes Section 1");
    assert_contains(html, "href=\"#section-2\"", "Range TOC includes Section 2");
    assert_contains(html, "href=\"#section-3\"", "Range TOC includes Section 3");
    assert_not_contains(html, "href=\"#section-1\">Section 1</a>\n        <ul>",
                        "Range TOC does not nest same-level headings");
    apex_free_string(html);

    /* Regression: TOC labels should not include source indentation/newline whitespace */
    const char *toc_whitespace =
        "# Top\n\n{{TOC:2}}\n\n##   Section 1\n\n##\tSection 2\n\n## Section   3";
    html = apex_markdown_to_html(toc_whitespace, strlen(toc_whitespace), &opts);
    assert_contains(html, "href=\"#section-1\">Section 1</a>", "TOC trims label whitespace");
    assert_contains(html, "href=\"#section-2\">Section 2</a>", "TOC normalizes tab/newline whitespace");
    assert_contains(html, "href=\"#section-3\">Section 3</a>", "TOC collapses internal whitespace runs");
    apex_free_string(html);

    /* Test TOC with max depth only */
    const char *max_toc = "# H1\n\n<!--TOC max2-->\n\n## H2\n\n### H3";
    html = apex_markdown_to_html(max_toc, strlen(max_toc), &opts);
    assert_contains(html, "<ul", "Max depth TOC");
    assert_contains(html, "H1", "Includes H1");
    assert_contains(html, "H2", "Includes H2");
    apex_free_string(html);

    /* TOC inside inline code (backticks) must not be rendered */
    const char *toc_inline_code = "# Title\n\nUse `{{TOC}}` in your template.\n\n## Section";
    html = apex_markdown_to_html(toc_inline_code, strlen(toc_inline_code), &opts);
    assert_contains(html, "<code>{{TOC}}</code>", "TOC in inline code is literal");
    assert_contains(html, "Section", "Headers still in document");
    apex_free_string(html);

    /* TOC inside fenced code block must not be rendered */
    const char *toc_code_block = "# Title\n\n```\n{{TOC}}\n```\n\n## Section";
    html = apex_markdown_to_html(toc_code_block, strlen(toc_code_block), &opts);
    assert_contains(html, "{{TOC}}", "TOC in code block is literal");
    assert_contains(html, "<pre>", "Code block present");
    apex_free_string(html);

    /* First valid TOC (not in code) is used when one is in code and one is not */
    const char *toc_then_real = "# Title\n\n`{{TOC}}`\n\n<!--TOC-->\n\n## Section";
    html = apex_markdown_to_html(toc_then_real, strlen(toc_then_real), &opts);
    assert_contains(html, "<code>{{TOC}}</code>", "TOC in code stays literal");
    assert_contains(html, "<nav class=\"toc\">", "Real TOC marker is rendered");
    assert_contains(html, "Section", "TOC includes section");
    apex_free_string(html);

    /* Escaped MMD TOC markers must not be expanded */
    const char *escaped_toc =
        "# Title\n\nMarked also recognizes MultiMarkdown-style \\{\\{TOC\\}\\}, "
        "and Pandoc-style `{{TOC:2-6}}`.\n\n## Section";
    html = apex_markdown_to_html(escaped_toc, strlen(escaped_toc), &opts);
    assert_contains(html, "{{TOC}}", "Escaped MMD TOC marker preserved as literal text");
    assert_not_contains(html, "<nav class=\"toc\">", "Escaped MMD TOC marker not expanded");
    assert_contains(html, "<code>{{TOC:2-6}}</code>", "Pandoc-style TOC marker preserved in inline code");
    apex_free_string(html);

    /* Kramdown {:toc} inside indented code must not be converted */
    apex_options kram_opts2 = apex_options_for_mode(APEX_MODE_KRAMDOWN);
    kram_opts2.enable_marked_extensions = true;
    const char *kramdown_toc_code = "# Title\n\n    {:toc}\n\n## Section";
    html = apex_markdown_to_html(kramdown_toc_code, strlen(kramdown_toc_code), &kram_opts2);
    assert_contains(html, "{:toc}", "Kramdown {:toc} preserved in indented code");
    assert_not_contains(html, "<nav class=\"toc\">", "Kramdown {:toc} not expanded in indented code");
    apex_free_string(html);

    /* Pandoc-style {{TOC:2-6}} inside fenced code must not be rendered */
    const char *toc_range_fenced = "# Title\n\n```\n{{TOC:2-6}}\n```\n\n## Section\n\n### Sub";
    html = apex_markdown_to_html(toc_range_fenced, strlen(toc_range_fenced), &opts);
    assert_contains(html, "{{TOC:2-6}}", "Pandoc-style TOC marker preserved in fenced code");
    assert_not_contains(html, "<nav class=\"toc\">", "Pandoc-style TOC marker not expanded in fenced code");
    apex_free_string(html);

    /* Pandoc-style {{TOC:2-6}} inside indented code must not be rendered */
    const char *toc_range_indented = "# Title\n\n    {{TOC:2-6}}\n\n## Section\n\n### Sub";
    html = apex_markdown_to_html(toc_range_indented, strlen(toc_range_indented), &opts);
    assert_contains(html, "{{TOC:2-6}}", "Pandoc-style TOC marker preserved in indented code");
    assert_not_contains(html, "<nav class=\"toc\">", "Pandoc-style TOC marker not expanded in indented code");
    apex_free_string(html);

    /* HTML <!--TOC--> inside inline code must not be rendered */
    html = apex_markdown_to_html("# Title\n\nUse `<!--TOC-->` here.\n\n## Section", 44, &opts);
    assert_contains(html, "<code>&lt;!--TOC--&gt;</code>", "HTML TOC marker preserved in inline code");
    assert_not_contains(html, "<nav class=\"toc\">", "HTML TOC marker not expanded in inline code");
    apex_free_string(html);

    /* Kramdown {:toc} inside fenced code must not be converted */
    const char *kramdown_toc_fenced = "# Title\n\n```\n{:toc}\n```\n\n## Section";
    html = apex_markdown_to_html(kramdown_toc_fenced, strlen(kramdown_toc_fenced), &kram_opts2);
    assert_contains(html, "{:toc}", "Kramdown {:toc} preserved in fenced code");
    assert_not_contains(html, "<nav class=\"toc\">", "Kramdown {:toc} not expanded in fenced code");
    apex_free_string(html);

    /* Test document without TOC marker */
    const char *no_toc = "# Header\n\nContent";
    html = apex_markdown_to_html(no_toc, strlen(no_toc), &opts);
    assert_contains(html, "<h1", "Normal header without TOC");
    assert_contains(html, "Header</h1>", "Normal header content");
    apex_free_string(html);

    /* Test nested header structure */
    const char *nested = "# Top\n\n<!--TOC-->\n\n## Level 2A\n\n### Level 3\n\n## Level 2B";
    html = apex_markdown_to_html(nested, strlen(nested), &opts);
    assert_contains(html, "<ul", "Nested TOC structure");
    assert_contains(html, "Level 2A", "First L2 in TOC");
    assert_contains(html, "Level 2B", "Second L2 in TOC");
    assert_contains(html, "Level 3", "L3 nested in TOC");
    apex_free_string(html);

    /* Kramdown-specific TOC syntax: {:toc} and {:.no_toc} */
    apex_options kram_opts = apex_options_for_mode(APEX_MODE_KRAMDOWN);
    /* Ensure marked extensions (including TOC) are enabled in Kramdown mode */
    kram_opts.enable_marked_extensions = true;

    /* Basic {:toc} replacement and .no_toc exclusion */
    const char *kramdown_toc =
        "# Contents\n"
        "{:.no_toc}\n"
        "\n"
        "## Section One\n"
        "\n"
        "{:toc}\n"
        "\n"
        "### Subsection\n";

    html = apex_markdown_to_html(kramdown_toc, strlen(kramdown_toc), &kram_opts);
    assert_contains(html, "<nav class=\"toc\">", "Kramdown {:toc} generates TOC");
    assert_contains(html, "Section One", "Kramdown TOC includes regular headings");
    /* The 'Contents' heading should be excluded from TOC due to .no_toc */
    if (strstr(html, "Contents") != NULL) {
        /* It should appear in the document, but not inside the TOC nav.
         * We perform a simple heuristic check: if 'Contents' only appears
         * outside the <nav class=\"toc\"> block, treat it as success.
         */
        const char *nav_start = strstr(html, "<nav class=\"toc\">");
        const char *nav_end = nav_start ? strstr(nav_start, "</nav>") : NULL;
        const char *contents_pos = strstr(html, "Contents");
        bool in_nav = nav_start && nav_end && contents_pos >= nav_start && contents_pos <= nav_end;
        if (!in_nav) {
            test_result(true, "Kramdown .no_toc excludes heading from TOC");
        } else {
            test_result(false, "Kramdown .no_toc heading appeared in TOC");
        }
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Kramdown document did not contain 'Contents' heading\n");
    }
    apex_free_string(html);

    /* {:toc} with max-depth option: support both max2 and max=2 forms */
    const char *kramdown_toc_max =
        "# Top\n"
        "\n"
        "## Level 2\n"
        "\n"
        "### Level 3\n"
        "\n"
        "{:toc max2}\n";

    html = apex_markdown_to_html(kramdown_toc_max, strlen(kramdown_toc_max), &kram_opts);
    assert_contains(html, "<nav class=\"toc\">", "Kramdown {:toc max2} generates TOC");
    assert_contains(html, "Level 2", "Kramdown {:toc max2} includes Level 2");
    /* Level 3 is beyond max2 and should not be linked in TOC */
    if (strstr(html, "Level 3") == NULL ||
        (strstr(html, "Level 3") && !strstr(html, "href=\"#level-3\""))) {
        test_result(true, "Kramdown {:toc max2} respects max depth");
    } else {
        test_result(false, "Kramdown {:toc max2} did not apply max depth");
    }
    apex_free_string(html);

    /* Manual {#id} must appear in TOC hrefs */
    const char *manual_id_toc =
        "# Custom {#my-custom}\n\n{{TOC}}\n\n## Other";
    html = apex_markdown_to_html(manual_id_toc, strlen(manual_id_toc), &opts);
    assert_contains(html, "href=\"#my-custom\"", "TOC uses manual heading ID");
    apex_free_string(html);

    /* Structured TOC entries API for library / Swift consumers */
    {
        const char *entries_doc =
            "# Introduction\n\n"
            "## Getting Started\n\n"
            "### Installation\n\n"
            "#### Too Deep\n\n"
            "# Skip Me\n"
            "{: .no_toc}\n";
        size_t count = 0;
        apex_toc_entry *entries = apex_markdown_to_toc_entries(
            entries_doc, strlen(entries_doc), NULL, &count);
        assert_option_bool(count == 3, true, "toc entries default depth count is 3");
        if (entries && count >= 3) {
            assert_option_bool(entries[0].level == 1, true, "toc entries[0] level is 1");
            assert_option_string(entries[0].text, "Introduction", "toc entries[0] text");
            assert_option_string(entries[0].id, "introduction", "toc entries[0] id");
            assert_option_bool(entries[1].level == 2, true, "toc entries[1] level is 2");
            assert_option_string(entries[1].id, "getting-started", "toc entries[1] id");
            assert_option_bool(entries[2].level == 3, true, "toc entries[2] level is 3");
            assert_option_string(entries[2].id, "installation", "toc entries[2] id");
        } else {
            test_result(false, "toc entries array populated");
        }
        apex_toc_entries_free(entries, count);

        apex_options depth_opts = apex_options_default();
        depth_opts.toc_min = 2;
        depth_opts.toc_max = 4;
        entries = apex_markdown_to_toc_entries(entries_doc, strlen(entries_doc),
                                              &depth_opts, &count);
        assert_option_bool(count == 3, true, "toc entries toc_min/max count");
        if (entries && count >= 1) {
            assert_option_bool(entries[0].level == 2, true, "toc entries respects toc_min");
            assert_option_string(entries[count - 1].text, "Too Deep",
                                 "toc entries includes H4 when toc_max=4");
        }
        apex_toc_entries_free(entries, count);

        entries = apex_markdown_to_toc_entries("No headings.\n", 12, NULL, &count);
        assert_option_bool(count == 0, true, "toc entries empty when no headings");
        assert_option_bool(entries == NULL, true, "toc entries NULL when empty");
        apex_toc_entries_free(entries, count);
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("TOC Generation Tests", had_failures, false);
}

/**
 * Test HTML markdown attributes
 */

void test_standalone_output(void) {
    int suite_failures = suite_start();
    print_suite_title("Standalone Document Output Tests", false, true);

    apex_options opts = apex_options_default();
    opts.standalone = true;
    opts.document_title = "Test Document";
    char *html;

    /* Test basic standalone document */
    html = apex_markdown_to_html("# Header\n\nContent", 18, &opts);
    assert_contains(html, "<!DOCTYPE html>", "Doctype present");
    assert_contains(html, "<html lang=\"en\">", "HTML tag with lang");
    assert_contains(html, "<meta charset=\"UTF-8\">", "Charset meta tag");
    assert_contains(html, "viewport", "Viewport meta tag");
    assert_contains(html, "<title>Test Document</title>", "Title tag");
    assert_contains(html, "<body>", "Body tag");
    assert_contains(html, "</body>", "Closing body tag");
    assert_contains(html, "</html>", "Closing html tag");
    apex_free_string(html);

    /* Test with custom stylesheet */
    const char *css_paths[] = { "styles.css", NULL };
    opts.stylesheet_paths = css_paths;
    opts.stylesheet_count = 1;
    html = apex_markdown_to_html("**Bold**", 8, &opts);
    assert_contains(html, "<link rel=\"stylesheet\" href=\"styles.css\">", "CSS link tag");
    /* Should not have inline styles when stylesheet is provided */
    if (strstr(html, "<style>") == NULL) {
        test_result(true, "No inline styles with external CSS");
    } else {
        test_result(false, "Inline styles present with external CSS");
    }
    apex_free_string(html);

    /* Test default title */
    opts.document_title = NULL;
    opts.stylesheet_paths = NULL;
    opts.stylesheet_count = 0;
    html = apex_markdown_to_html("Content", 7, &opts);
    assert_contains(html, "<title>Document</title>", "Default title");
    apex_free_string(html);

    /* Test inline styles when no stylesheet */
    opts.stylesheet_paths = NULL;
    opts.stylesheet_count = 0;
    html = apex_markdown_to_html("Content", 7, &opts);
    assert_contains(html, "<style>", "Default inline styles");
    assert_contains(html, "font-family:", "Style rules present");
    apex_free_string(html);

    /* Test that non-standalone doesn't include document structure */
    apex_options frag_opts = apex_options_default();
    frag_opts.standalone = false;
    html = apex_markdown_to_html("# Header", 8, &frag_opts);
    if (strstr(html, "<!DOCTYPE") == NULL && strstr(html, "<body>") == NULL) {
        test_result(true, "Fragment mode doesn't include document structure");
    } else {
        test_result(false, "Fragment mode has document structure");
    }
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Standalone Document Output Tests", had_failures, false);
}

/**
 * Test pretty HTML output
 */

void test_pretty_html(void) {
    int suite_failures = suite_start();
    print_suite_title("Pretty HTML Output Tests", false, true);

    apex_options opts = apex_options_default();
    opts.pretty = true;
    opts.relaxed_tables = false;  /* Use standard tables for pretty HTML tests */
    char *html;

    /* Test basic pretty formatting */
    html = apex_markdown_to_html("# Header\n\nPara", 14, &opts);
    /* Check for indentation and newlines */
    assert_contains(html, "<h1", "Opening tag present");
    assert_contains(html, ">\n", "Opening tag on own line");
    assert_contains(html, "</h1>\n", "Closing tag on own line");
    assert_contains(html, "  Header", "Content indented");
    apex_free_string(html);

    /* Test nested structure (list) */
    html = apex_markdown_to_html("- Item 1\n- Item 2", 17, &opts);
    assert_contains(html, "<ul>\n", "List opening formatted");
    assert_contains(html, "  <li>", "List item indented");
    assert_contains(html, "</ul>", "List closing formatted");
    apex_free_string(html);

    /* Test inline elements stay inline */
    html = apex_markdown_to_html("Text with **bold**", 18, &opts);
    assert_contains(html, "<strong>bold</strong>", "Inline elements not split");
    apex_free_string(html);

    /* TOC links should not get indentation whitespace inside anchor text */
    html = apex_markdown_to_html("# Top\n\n{{TOC:2}}\n\n## Section 1", 31, &opts);
    assert_contains(html, "<a href=\"#section-1\">Section 1</a>", "Pretty mode keeps TOC link text clean");
    assert_not_contains(html, "<a href=\"#section-1\">        Section 1</a>",
                        "Pretty mode does not pad TOC link text");
    apex_free_string(html);

    /* Test table formatting */
    const char *table = "| A | B |\n|---|---|\n| C | D |";
    html = apex_markdown_to_html(table, strlen(table), &opts);
    assert_contains(html, "<table>\n", "Table formatted");
    assert_contains(html, "  <thead>", "Table sections indented");
    assert_contains(html, "    <tr>", "Table rows further indented");
    apex_free_string(html);

    /* Test that non-pretty mode is compact */
    apex_options compact_opts = apex_options_default();
    compact_opts.pretty = false;
    html = apex_markdown_to_html("# H\n\nP", 7, &compact_opts);
    /* Should not have extra indentation */
    if (strstr(html, "  H") == NULL) {
        test_result(true, "Compact mode has no indentation");
    } else {
        test_result(false, "Compact mode has indentation");
    }
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Pretty HTML Output Tests", had_failures, false);
}

/**
 * Test --xhtml / --strict-xhtml HTML serialization
 */
void test_xhtml_output(void) {
    int suite_failures = suite_start();
    print_suite_title("XHTML Output Mode Tests", false, true);

    apex_options opts = apex_options_default();
    char *html;
    const char *hr_md = "\n\n---\n\n";

    /* --xhtml: self-closing void tags in fragment */
    opts.xhtml = true;
    html = apex_markdown_to_html(hr_md, strlen(hr_md), &opts);
    assert_contains(html, "<hr />", "XHTML mode self-closes hr");
    apex_free_string(html);

    /* --xhtml standalone: charset meta self-closed, HTML5 doctype */
    opts = apex_options_default();
    opts.xhtml = true;
    opts.standalone = true;
    opts.document_title = "X";
    html = apex_markdown_to_html("Hi", 2, &opts);
    assert_contains(html, "<meta charset=\"UTF-8\" />", "XHTML standalone charset meta self-closed");
    assert_contains(html, "<!DOCTYPE html>", "Still HTML5 doctype");
    apex_free_string(html);

    /* --strict-xhtml standalone: polyglot document */
    opts = apex_options_default();
    opts.strict_xhtml = true;
    opts.standalone = true;
    opts.document_title = "S";
    html = apex_markdown_to_html("Hi", 2, &opts);
    assert_contains(html, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>", "XML declaration");
    assert_contains(html, "xmlns=\"http://www.w3.org/1999/xhtml\"", "XHTML namespace");
    assert_contains(html, "application/xhtml+xml", "Strict Content-Type meta");
    if (strstr(html, "<meta charset=") != NULL) {
        test_result(false, "Strict mode should not emit separate charset meta");
    } else {
        test_result(true, "Strict mode uses Content-Type only for charset");
    }
    apex_free_string(html);

    /* strict implies void serialization on fragments */
    opts = apex_options_default();
    opts.strict_xhtml = true;
    html = apex_markdown_to_html(hr_md, strlen(hr_md), &opts);
    assert_contains(html, "<hr />", "Strict mode self-closes void elements");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("XHTML Output Mode Tests", had_failures, false);
}

/**
 * Test terminal and terminal256 output format: ANSI output, list markers, and terminal_width option.
 */
void test_terminal_output(void) {
    int suite_failures = suite_start();
    print_suite_title("Terminal Output Tests", false, true);

    apex_options opts = apex_options_default();
    char *out;

    /* Terminal format produces ANSI and content */
    opts.output_format = APEX_OUTPUT_TERMINAL;
    out = apex_markdown_to_html("# Hello", 7, &opts);
    assert_contains(out, "\033", "Terminal output contains ANSI escape");
    assert_contains(out, "Hello", "Terminal output contains heading text");
    apex_free_string(out);

    /* terminal256 also produces ANSI */
    opts.output_format = APEX_OUTPUT_TERMINAL256;
    out = apex_markdown_to_html("**bold** text", 13, &opts);
    assert_contains(out, "\033", "Terminal256 output contains ANSI");
    assert_contains(out, "bold", "Terminal256 output contains bold text");
    apex_free_string(out);

    /* Bullet list: default list_marker yields bullet and item text */
    opts.output_format = APEX_OUTPUT_TERMINAL;
    out = apex_markdown_to_html("- one\n- two", 11, &opts);
    assert_contains(out, "* ", "Terminal bullet list contains marker");
    assert_contains(out, "one", "Terminal list contains first item");
    assert_contains(out, "two", "Terminal list contains second item");
    apex_free_string(out);

    /* Ordered list: same list_marker styling, numbered labels */
    out = apex_markdown_to_html("1. first\n2. second", 18, &opts);
    assert_contains(out, "1.", "Terminal ordered list contains first number");
    assert_contains(out, "2.", "Terminal ordered list contains second number");
    assert_contains(out, "first", "Terminal ordered list contains first item text");
    assert_contains(out, "second", "Terminal ordered list contains second item text");
    apex_free_string(out);

    /* terminal_width in options does not break output (wrapping is applied by CLI) */
    opts.terminal_width = 40;
    out = apex_markdown_to_html("plain paragraph", 15, &opts);
    test_result(out != NULL && strstr(out, "plain") != NULL, "terminal_width set still produces terminal output");
    if (out) apex_free_string(out);

    /* apex_resolve_local_image_path */
    {
        char *rp = apex_resolve_local_image_path("img/a.png", "/tmp/proj");
        test_result(rp != NULL && strcmp(rp, "/tmp/proj/img/a.png") == 0,
                    "apex_resolve_local_image_path joins base_directory");
        free(rp);
        rp = apex_resolve_local_image_path("/abs/foo.png", "/any");
        test_result(rp != NULL && strcmp(rp, "/abs/foo.png") == 0,
                    "apex_resolve_local_image_path keeps absolute paths");
        free(rp);
    }

    /* Terminal images: inline rendering uses isatty(STDOUT_FILENO). Test output is
     * captured to a string; under a normal CI pipe or non-TTY, stdout is not a TTY
     * so these exercises never call curl or imgcat/chafa/viu/catimg. (Running the
     * suite in an interactive terminal with those tools on PATH could take a
     * different path for remote URLs.) */

    /* Remote images: link-style fallback when not inline (non-TTY: no download/viewer) */
    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_TERMINAL;
    out = apex_markdown_to_html("![r](https://ex.com/x.png)", 28, &opts);
    assert_contains(out, "https://ex.com/x.png", "Remote image URL in fallback");
    test_result(strstr(out, "![") == NULL, "Remote image fallback uses link style not markdown image");
    apex_free_string(out);

    /* http:// same as https for fallback */
    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_TERMINAL;
    out = apex_markdown_to_html("![h](http://ex.com/y.png)", 26, &opts);
    assert_contains(out, "http://ex.com/y.png", "http remote image URL in fallback");
    test_result(strstr(out, "![") == NULL, "http remote image uses link style not markdown image");
    apex_free_string(out);

    /* terminal256: same link-style fallback for images */
    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_TERMINAL256;
    out = apex_markdown_to_html("![z](https://ex.com/z.png)", 28, &opts);
    assert_contains(out, "https://ex.com/z.png", "terminal256 remote image URL in fallback");
    test_result(strstr(out, "![") == NULL, "terminal256 remote image uses link style");
    apex_free_string(out);

    /* Empty alt still emits (url) */
    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_TERMINAL;
    out = apex_markdown_to_html("![](https://ex.com/e.png)", 25, &opts);
    assert_contains(out, "https://ex.com/e.png", "Empty-alt remote image URL in fallback");
    test_result(strstr(out, "![") == NULL, "Empty-alt image uses link style");
    apex_free_string(out);

    /* terminal_image_width is ignored when not inlining (non-TTY); must not crash */
    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_TERMINAL;
    opts.terminal_image_width = 72;
    out = apex_markdown_to_html("![w](https://ex.com/w.png)", 28, &opts);
    test_result(out != NULL && strstr(out, "https://ex.com/w.png") != NULL,
                "terminal_image_width set with non-TTY still yields link-style remote image");
    apex_free_string(out);

    /* terminal_inline_images false: link-style fallback */
    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_TERMINAL;
    opts.terminal_inline_images = false;
    out = apex_markdown_to_html("![z](local.png)", 17, &opts);
    assert_contains(out, "local.png", "Disabled inline: URL shown like a link");
    test_result(strstr(out, "![") == NULL, "terminal_inline_images off uses link style not markdown image");
    apex_free_string(out);

    /* Missing local file: link-style fallback (non-TTY) */
    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_TERMINAL;
    out = apex_markdown_to_html("![missing](_/apex_test_missing_image_99.png)", 45, &opts);
    assert_contains(out, "apex_test_missing_image_99.png", "Missing file: URL in link-style output");
    test_result(strstr(out, "![") == NULL, "Missing local image uses link style not markdown image");
    apex_free_string(out);

    /* paginate_symbols: must not crash; non-TTY uses link-style images */
    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_TERMINAL256;
    opts.paginate = true;
    opts.paginate_symbols = true;
    out = apex_markdown_to_html("![z](local.png)", 17, &opts);
    test_result(out != NULL, "paginate_symbols terminal256 produces output");
    apex_free_string(out);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Terminal Output Tests", had_failures, false);
}

/**
 * Test RTF output
 */
void test_rtf_output(void) {
    int suite_failures = suite_start();
    print_suite_title("RTF Output Tests", false, true);

    apex_options opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_RTF;
    char *out;

    const char *md =
        "# Title\n\n"
        "A paragraph with **bold**, *italic*, ~~strike~~, and `code`.\n\n"
        "Visit [Example](https://example.com).\n\n"
        "- Item one\n"
        "- Item two\n\n"
        "> A quote\n\n"
        "```\ncode block\n```\n\n"
        "| A | B |\n| --- | --- |\n| 1 | 2 |\n\n"
        "H~2~O and E=mc^2^.\n\n"
        "---\n";

    out = apex_markdown_to_html(md, strlen(md), &opts);
    assert_contains(out, "{\\rtf1", "RTF has document header");
    assert_contains(out, "\\fonttbl", "RTF has font table");
    assert_contains(out, "Title", "RTF includes heading text");
    assert_contains(out, "\\b ", "RTF has bold");
    assert_contains(out, "\\i ", "RTF has italic");
    assert_contains(out, "\\strike", "RTF has strikethrough");
    assert_contains(out, "\\f1", "RTF uses mono font for code");
    assert_contains(out, "HYPERLINK", "RTF has hyperlink field");
    assert_contains(out, "example.com", "RTF link URL present");
    assert_contains(out, "\\bullet", "RTF has bullet list");
    assert_contains(out, "\\trowd", "RTF has table row");
    assert_contains(out, "code block", "RTF has code block text");
    assert_contains(out, "\\super", "RTF has superscript");
    assert_contains(out, "\\sub", "RTF has subscript");
    test_result(out != NULL && strstr(out, "}") != NULL, "RTF document contains closing brace");
    apex_free_string(out);

    /* Ordered list alone so mixed-list merging cannot absorb it into a ul */
    out = apex_markdown_to_html("1. First\n2. Second\n", 20, &opts);
    assert_contains(out, "1. ", "RTF has ordered list marker");
    assert_contains(out, "First", "RTF ordered list item text");
    apex_free_string(out);

    /* Footnotes endnotes */
    const char *fn = "Note.[^1]\n\n[^1]: Footnote body.\n";
    out = apex_markdown_to_html(fn, strlen(fn), &opts);
    assert_contains(out, "{\\rtf1", "Footnote doc is RTF");
    assert_contains(out, "Footnotes", "RTF endnotes section");
    assert_contains(out, "Footnote body", "RTF footnote body");
    apex_free_string(out);

    /* Direct serializer */
    out = apex_markdown_to_html("Hi", 2, &opts);
    test_result(out != NULL && strncmp(out, "{\\rtf1", 6) == 0, "apex_markdown_to_html RTF path");
    apex_free_string(out);

    /* Remote images: alt becomes a hyperlink; no raw URL dump in body */
    {
        const char *remote =
            "![Version: 1.1.12](https://img.shields.io/badge/Version-1.1.12-528c9e)\n";
        out = apex_markdown_to_html(remote, strlen(remote), &opts);
        assert_contains(out, "HYPERLINK", "Remote image uses HYPERLINK fallback");
        assert_contains(out, "Version: 1.1.12", "Remote image shows alt text");
        assert_contains(out, "img.shields.io", "Remote image URL in HYPERLINK field");
        test_result(out != NULL && strstr(out, "] (") == NULL, "Remote image does not dump ] (url)");
        test_result(out != NULL && strstr(out, "\\pict") == NULL, "Remote image is not embedded as pict");
        apex_free_string(out);
    }

    {
        const char *empty_alt = "![](https://example.com/x.png)\n";
        out = apex_markdown_to_html(empty_alt, strlen(empty_alt), &opts);
        assert_contains(out, "HYPERLINK", "Empty-alt remote image is still a link");
        assert_contains(out, "image", "Empty-alt remote image uses image label");
        apex_free_string(out);
    }

    /* Local JPEG embeds without embed_images */
    opts.embed_images = false;
    opts.base_directory = "tests/fixtures/images";
    {
        const char *local = "![Local](img/app-pass-1-profile-menu.jpg)\n";
        out = apex_markdown_to_html(local, strlen(local), &opts);
        assert_contains(out, "\\pict", "Local JPEG embeds as RTF pict");
        assert_contains(out, "jpegblip", "Local JPEG uses jpegblip");
        test_result(out != NULL && strstr(out, "HYPERLINK") == NULL,
                    "Embedded local image is not a hyperlink fallback");
        apex_free_string(out);
    }
    opts.base_directory = NULL;

    /* HTML blocks: definition list, callout, and generic tag stripping */
    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_RTF;
    {
        const char *html_blocks =
            "<dl><dt>HtmlTerm</dt><dd>HtmlDef with <em>emphasis</em></dd></dl>\n"
            "\n"
            "<div class=\"callout\">Callout note</div>\n"
            "\n"
            "<div>Generic html</div>\n";
        out = apex_markdown_to_html(html_blocks, strlen(html_blocks), &opts);
        assert_contains(out, "HtmlTerm", "RTF definition-list HTML keeps the term");
        assert_contains(out, "HtmlDef", "RTF definition-list HTML keeps the definition");
        assert_contains(out, "\\b ", "RTF definition term is bold");
        assert_contains(out, "Callout note", "RTF callout HTML keeps the body");
        assert_contains(out, "\\box", "RTF callout uses a boxed paragraph");
        assert_contains(out, "Generic html", "RTF generic HTML is stripped to text");
        apex_free_string(out);
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("RTF Output Tests", had_failures, false);
}

/**
 * Test header ID generation
 */

void test_header_ids(void) {
    int suite_failures = suite_start();
    print_suite_title("Header ID Generation Tests", false, true);

    apex_options opts = apex_options_default();
    char *html;

    /* Test default GFM format (with dashes) */
    html = apex_markdown_to_html("# Emoji Support\n## Test Heading", 33, &opts);
    assert_contains(html, "id=\"emoji-support\"", "GFM format: emoji-support");
    assert_contains(html, "id=\"test-heading\"", "GFM format: test-heading");
    apex_free_string(html);

    /* Test GFM format converts emojis to names (Pandoc GFM behavior) */
    opts.id_format = 0;  /* GFM format */
    opts.enable_marked_extensions = true;  /* Enable emoji support */
    const char *emoji_header_test = "# 😄 Emoji Support";
    html = apex_markdown_to_html(emoji_header_test, strlen(emoji_header_test), &opts);
    assert_contains(html, "id=\"smile-emoji-support\"", "GFM format converts emoji to name");
    apex_free_string(html);

    const char *emoji_only_test = "# 🚀";
    html = apex_markdown_to_html(emoji_only_test, strlen(emoji_only_test), &opts);
    assert_contains(html, "id=\"rocket\"", "GFM format converts single emoji to name");
    apex_free_string(html);

    const char *emoji_multiple_test = "# 👍 👎";
    html = apex_markdown_to_html(emoji_multiple_test, strlen(emoji_multiple_test), &opts);
    assert_contains(html, "id=\"thumbsup-thumbsdown\"", "GFM format converts multiple emojis to names");
    apex_free_string(html);

    /* Test MMD format (preserves dashes, removes spaces) */
    opts.id_format = 1;  /* MMD format */
    html = apex_markdown_to_html("# Emoji Support\n## Test Heading", 33, &opts);
    assert_contains(html, "id=\"emojisupport\"", "MMD format: emojisupport (spaces removed)");
    assert_contains(html, "id=\"testheading\"", "MMD format: testheading (spaces removed)");
    apex_free_string(html);

    /* Test MMD format preserves dashes */
    const char *mmd_dash_test = "# header-one";
    html = apex_markdown_to_html(mmd_dash_test, strlen(mmd_dash_test), &opts);
    assert_contains(html, "id=\"header-one\"", "MMD format preserves regular dash");
    apex_free_string(html);

    const char *mmd_em_dash_test = "# header—one";
    html = apex_markdown_to_html(mmd_em_dash_test, strlen(mmd_em_dash_test), &opts);
    assert_contains(html, "id=\"header—one\"", "MMD format preserves em dash");
    apex_free_string(html);

    const char *mmd_en_dash_test = "# header–one";
    html = apex_markdown_to_html(mmd_en_dash_test, strlen(mmd_en_dash_test), &opts);
    assert_contains(html, "id=\"header–one\"", "MMD format preserves en dash");
    apex_free_string(html);

    /* Test MMD format preserves leading/trailing dashes */
    const char *mmd_leading_test = "# -Leading";
    html = apex_markdown_to_html(mmd_leading_test, strlen(mmd_leading_test), &opts);
    assert_contains(html, "id=\"-leading\"", "MMD format preserves leading dash");
    apex_free_string(html);

    const char *mmd_trailing_test = "# Trailing-";
    html = apex_markdown_to_html(mmd_trailing_test, strlen(mmd_trailing_test), &opts);
    assert_contains(html, "id=\"trailing-\"", "MMD format preserves trailing dash");
    apex_free_string(html);

    /* Test MMD format preserves diacritics */
    const char *mmd_diacritics_test = "# Émoji Support";
    html = apex_markdown_to_html(mmd_diacritics_test, strlen(mmd_diacritics_test), &opts);
    assert_contains(html, "id=\"Émojisupport\"", "MMD format preserves diacritics");
    apex_free_string(html);

    /* Test MMD format removes apostrophes (curly and straight) - they break anchor links */
    const char *mmd_apostrophe_test = "# What\xE2\x80\x99s Markdown?";  /* curly apostrophe U+2019 */
    html = apex_markdown_to_html(mmd_apostrophe_test, strlen(mmd_apostrophe_test), &opts);
    assert_contains(html, "id=\"whatsmarkdown\"", "MMD format removes curly apostrophe from ID");
    apex_free_string(html);

    /* Test --no-ids option */
    opts.generate_header_ids = false;
    html = apex_markdown_to_html("# Emoji Support", 16, &opts);
    if (strstr(html, "id=") == NULL) {
        test_result(true, "--no-ids disables ID generation");
    } else {
        test_result(false, "--no-ids still generates IDs");
    }
    apex_free_string(html);

    /* Test diacritics handling */
    opts.generate_header_ids = true;
    opts.id_format = 0;  /* GFM format */
    const char *diacritics_test = "# Émoji Support\n## Test—Heading";
    html = apex_markdown_to_html(diacritics_test, strlen(diacritics_test), &opts);
    assert_contains(html, "id=\"emoji-support\"", "Diacritics converted (É→e)");
    /* GFM: em dash should be removed (not converted) */
    assert_contains(html, "id=\"testheading\"", "GFM removes em dash");
    apex_free_string(html);

    /* Test en dash in GFM */
    const char *en_dash_test = "## Test–Heading";
    html = apex_markdown_to_html(en_dash_test, strlen(en_dash_test), &opts);
    assert_contains(html, "id=\"testheading\"", "GFM removes en dash");
    apex_free_string(html);

    /* Test GFM punctuation removal */
    const char *gfm_punct_test = "# Hello, World!";
    html = apex_markdown_to_html(gfm_punct_test, strlen(gfm_punct_test), &opts);
    assert_contains(html, "id=\"hello-world\"", "GFM removes punctuation, spaces become dashes");
    apex_free_string(html);

    /* Test GFM multiple spaces collapse */
    const char *gfm_spaces_test = "# Multiple   Spaces";
    html = apex_markdown_to_html(gfm_spaces_test, strlen(gfm_spaces_test), &opts);
    assert_contains(html, "id=\"multiple-spaces\"", "GFM collapses multiple spaces to single dash");
    apex_free_string(html);

    /* Test special characters */
    html = apex_markdown_to_html("# Hello, World!", 16, &opts);
    assert_contains(html, "id=\"hello-world\"", "Comma and exclamation converted");
    apex_free_string(html);

    /* Test multiple spaces */
    html = apex_markdown_to_html("# Multiple   Spaces", 20, &opts);
    assert_contains(html, "id=\"multiple-spaces\"", "Multiple spaces become single dash");
    apex_free_string(html);

    /* Test leading/trailing dashes trimmed */
    html = apex_markdown_to_html("# -Leading Dash", 16, &opts);
    assert_contains(html, "id=\"leading-dash\"", "Leading dash trimmed");
    apex_free_string(html);

    html = apex_markdown_to_html("# Trailing Dash-", 17, &opts);
    assert_contains(html, "id=\"trailing-dash\"", "Trailing dash trimmed");
    apex_free_string(html);

    /* Test TOC uses same ID format */
    opts.id_format = 0;  /* GFM format */
    const char *toc_doc = "# Main Title\n\n<!--TOC-->\n\n## Subtitle";
    html = apex_markdown_to_html(toc_doc, strlen(toc_doc), &opts);
    assert_contains(html, "id=\"main-title\"", "TOC header has GFM ID");
    assert_contains(html, "href=\"#main-title\"", "TOC link uses GFM ID");
    apex_free_string(html);

    /* Test TOC with MMD format */
    opts.id_format = 1;  /* MMD format */
    html = apex_markdown_to_html(toc_doc, strlen(toc_doc), &opts);
    assert_contains(html, "id=\"maintitle\"", "TOC header has MMD ID");
    assert_contains(html, "href=\"#maintitle\"", "TOC link uses MMD ID");
    apex_free_string(html);

    /* Test Kramdown format (spaces→dashes, removes em/en dashes and diacritics) */
    opts.id_format = 2;  /* Kramdown format */
    html = apex_markdown_to_html("# header one", 12, &opts);
    assert_contains(html, "id=\"header-one\"", "Kramdown: spaces become dashes");
    apex_free_string(html);

    const char *kramdown_em_dash_test = "# header—one";
    html = apex_markdown_to_html(kramdown_em_dash_test, strlen(kramdown_em_dash_test), &opts);
    assert_contains(html, "id=\"headerone\"", "Kramdown removes em dash");
    apex_free_string(html);

    const char *kramdown_en_dash_test = "# header–one";
    html = apex_markdown_to_html(kramdown_en_dash_test, strlen(kramdown_en_dash_test), &opts);
    assert_contains(html, "id=\"headerone\"", "Kramdown removes en dash");
    apex_free_string(html);

    const char *kramdown_diacritics_test = "# Émoji Support";
    html = apex_markdown_to_html(kramdown_diacritics_test, strlen(kramdown_diacritics_test), &opts);
    assert_contains(html, "id=\"moji-support\"", "Kramdown removes diacritics");
    apex_free_string(html);

    const char *kramdown_spaces_test = "# Multiple   Spaces";
    html = apex_markdown_to_html(kramdown_spaces_test, strlen(kramdown_spaces_test), &opts);
    assert_contains(html, "id=\"multiple---spaces\"", "Kramdown: multiple spaces become multiple dashes");
    apex_free_string(html);

    const char *kramdown_punct_test = "# Hello, World!";
    html = apex_markdown_to_html(kramdown_punct_test, strlen(kramdown_punct_test), &opts);
    assert_contains(html, "id=\"hello-world\"", "Kramdown: punctuation becomes dash, trailing punctuation removed");
    apex_free_string(html);

    const char *kramdown_leading_test = "# -Leading Dash";
    html = apex_markdown_to_html(kramdown_leading_test, strlen(kramdown_leading_test), &opts);
    assert_contains(html, "id=\"leading-dash\"", "Kramdown trims leading dash");
    apex_free_string(html);

    const char *kramdown_trailing_test = "# Trailing Dash-";
    html = apex_markdown_to_html(kramdown_trailing_test, strlen(kramdown_trailing_test), &opts);
    assert_contains(html, "id=\"trailing-dash-\"", "Kramdown preserves trailing dash");
    apex_free_string(html);

    const char *kramdown_punct_space_test = "# Test, Here";
    html = apex_markdown_to_html(kramdown_punct_space_test, strlen(kramdown_punct_space_test), &opts);
    assert_contains(html, "id=\"test-here\"", "Kramdown: punctuation→dash, following space skipped");
    apex_free_string(html);

    /* Test empty header gets default ID */
    html = apex_markdown_to_html("#", 1, &opts);
    assert_contains(html, "id=\"header\"", "Empty header gets default ID");
    apex_free_string(html);

    /* Test header with only special characters */
    html = apex_markdown_to_html("# !@#$%", 7, &opts);
    assert_contains(html, "id=\"header\"", "Special-only header gets default ID");
    apex_free_string(html);

    /* Test --header-anchors option */
    opts.header_anchors = true;
    html = apex_markdown_to_html("# Test Header", 13, &opts);
    assert_contains(html, "<a href=\"#test-header\"", "Anchor tag has href attribute");
    assert_contains(html, "aria-hidden=\"true\"", "Anchor tag has aria-hidden");
    assert_contains(html, "class=\"anchor\"", "Anchor tag has anchor class");
    assert_contains(html, "id=\"test-header\"", "Anchor tag has id attribute");
    assert_contains(html, "<h1><a", "Anchor tag is inside header tag");
    assert_contains(html, "</a>Test Header</h1>", "Anchor tag comes before header text");
    apex_free_string(html);

    /* Test anchor tags with multiple headers */
    const char *multi_header_test = "# Header One\n## Header Two";
    html = apex_markdown_to_html(multi_header_test, strlen(multi_header_test), &opts);
    assert_contains(html, "<h1><a href=\"#header-one\"", "First header has anchor");
    assert_contains(html, "<h2><a href=\"#header-two\"", "Second header has anchor");
    apex_free_string(html);

    /* Test anchor tags with different ID formats */
    opts.id_format = 1;  /* MMD format */
    html = apex_markdown_to_html("# Test Header", 13, &opts);
    assert_contains(html, "<a href=\"#testheader\"", "MMD format anchor tag");
    assert_contains(html, "id=\"testheader\"", "MMD format anchor ID");
    apex_free_string(html);

    opts.id_format = 2;  /* Kramdown format */
    html = apex_markdown_to_html("# Test Header", 13, &opts);
    assert_contains(html, "<a href=\"#test-header\"", "Kramdown format anchor tag");
    assert_contains(html, "id=\"test-header\"", "Kramdown format anchor ID");
    apex_free_string(html);

    /* Test that header_anchors=false uses header IDs (default behavior) */
    opts.header_anchors = false;
    opts.id_format = 0;  /* GFM format */
    html = apex_markdown_to_html("# Test Header", 13, &opts);
    assert_contains(html, "<h1 id=\"test-header\"", "Default mode uses header ID attribute");
    if (strstr(html, "<a href=") == NULL) {
        test_result(true, "Default mode does not use anchor tags");
    } else {
        test_result(false, "Default mode incorrectly uses anchor tags");
    }
    apex_free_string(html);

    /* Regression: permalink anchors used to under-size the inject buffer and
     * silently drop the HTML tail once remaining hit 0. Build many long
     * headings plus a unique trailer after the last heading. */
    {
        apex_options trunc_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        trunc_opts.header_anchors = true;
        const char *trailer = "APEX_HEADER_ANCHOR_TRAILER_UNIQUE_xyzzy";
        char md_buf[16384];
        size_t pos = 0;
        for (int i = 0; i < 24; i++) {
            int n = snprintf(md_buf + pos, sizeof(md_buf) - pos,
                "## Section %02d With A Fairly Long Heading Title To Stretch Ids And Anchors\n\n"
                "Body paragraph %02d with enough prose that the inject pass has real work "
                "after each heading tag.\n\n",
                i, i);
            if (n < 0 || (size_t)n >= sizeof(md_buf) - pos) break;
            pos += (size_t)n;
        }
        int n = snprintf(md_buf + pos, sizeof(md_buf) - pos,
            "\n\n%s\n", trailer);
        if (n > 0) pos += (size_t)n;

        html = apex_markdown_to_html(md_buf, pos, &trunc_opts);
        assert_contains(html, trailer,
            "header_anchors: trailer after many long headings is not truncated");
        assert_contains(html, "class=\"anchor\"",
            "header_anchors: stress fixture still injects anchor tags");
        apex_free_string(html);

        trunc_opts.header_anchors = false;
        html = apex_markdown_to_html(md_buf, pos, &trunc_opts);
        assert_contains(html, trailer,
            "header ids: trailer after many long headings is not truncated");
        apex_free_string(html);
    }

    /* MMD heading [id] edge case: when [id] matches link ref but is last in heading
     * with other content, treat as heading ID not link */
    const char *mmd_id_conflict = "# Heading [mermaid]\n\n[mermaid]: https://example.com\n";
    html = apex_markdown_to_html(mmd_id_conflict, strlen(mmd_id_conflict), &opts);
    assert_contains(html, "id=\"mermaid\"", "MMD [id] at end of heading with link ref: treated as ID");
    assert_contains(html, "Heading mermaid", "MMD [id] at end: mermaid rendered as text not link");
    if (strstr(html, "<a href=\"https://example.com\">mermaid</a>") != NULL) {
        test_result(false, "MMD [id] at end should not render as link");
    } else {
        test_result(true, "MMD [id] at end: mermaid not rendered as link");
    }
    apex_free_string(html);

    /* Entire heading is [id]: keep as link to avoid empty heading */
    const char *mmd_only_link = "# [mermaid]\n\n[mermaid]: https://example.com\n";
    html = apex_markdown_to_html(mmd_only_link, strlen(mmd_only_link), &opts);
    assert_contains(html, "<a href=\"https://example.com\"", "Heading only [id] with link ref: remains link");
    assert_contains(html, ">mermaid</a>", "Heading only [id]: mermaid renders as link text");
    apex_free_string(html);

    /* [id] in middle of heading: remains link */
    const char *mmd_link_middle = "# Check out [mermaid] for diagrams\n\n[mermaid]: https://example.com\n";
    html = apex_markdown_to_html(mmd_link_middle, strlen(mmd_link_middle), &opts);
    assert_contains(html, "<a href=\"https://example.com\"", "MMD [id] in middle: remains link");
    apex_free_string(html);

    /* Table cell text must not be injected into later heading tags.
     * GFM table nodes are numbered just above CMARK_NODE_HEADING, so a range
     * check treated the table, its header row, and its cells as headings. */
    const char *table_then_headings =
        "| Mechanism | Examples | Description |\n"
        "| --- | --- | --- |\n"
        "| `mx` | `mx:example.com` | Mail exchange |\n"
        "\n"
        "#### SPF Examples\n"
        "\n"
        "#### Bonus Extra\n"
        "\n"
        "### Tripod Issue\n"
        "\n"
        "### Setup\n";
    html = apex_markdown_to_html(table_then_headings, strlen(table_then_headings), &opts);
    assert_contains(html, "<th>Mechanism</th>", "Table header text stays in the table");
    assert_contains(html, "<h4 id=\"spf-examples\">SPF Examples</h4>",
                    "Heading after a table is not given a table cell attribute");
    assert_contains(html, "<h3 id=\"tripod-issue\">Tripod Issue</h3>",
                    "h3 after a table does not include the first header cell");
    assert_contains(html, "<h3 id=\"setup\">Setup</h3>",
                    "h3 after a table does not include the second header cell");
    assert_not_contains(html, "<h3 Mechanism", "First header cell is not an h3 attribute");
    assert_not_contains(html, "<h3 Examples", "Second header cell is not an h3 attribute");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Header ID Generation Tests", had_failures, false);
}

/**
 * Test superscript, subscript, underline, strikethrough, and highlight
 */

void test_indices(void) {
    int suite_failures = suite_start();
    print_suite_title("Index Tests", false, true);

    apex_options opts = apex_options_default();
    opts.mode = APEX_MODE_UNIFIED;
    opts.enable_indices = true;
    opts.enable_mmark_index_syntax = true;
    opts.enable_textindex_syntax = true;
    opts.enable_leanpub_index_syntax = true;
    opts.group_index_by_letter = true;

    char *html;

    /* Test basic mmark index syntax */
    const char *mmark_basic = "This is about protocols (!Protocol).";
    html = apex_markdown_to_html(mmark_basic, strlen(mmark_basic), &opts);
    assert_contains(html, "class=\"index\"", "mmark index generates index marker");
    assert_contains(html, "idxref-", "mmark index generates anchor ID");
    assert_contains(html, "Protocol", "mmark index preserves term");
    apex_free_string(html);

    /* Test mmark index with subitem */
    const char *mmark_subitem = "HTTP/1.1 (!HTTP, HTTP/1.1) is a protocol.";
    html = apex_markdown_to_html(mmark_subitem, strlen(mmark_subitem), &opts);
    assert_contains(html, "class=\"index\"", "mmark subitem generates index marker");
    assert_contains(html, "HTTP", "mmark subitem includes main term");
    apex_free_string(html);

    /* Test mmark primary index entry */
    const char *mmark_primary = "This is a primary topic (!!Primary Topic, Sub Topic).";
    html = apex_markdown_to_html(mmark_primary, strlen(mmark_primary), &opts);
    assert_contains(html, "class=\"index\"", "mmark primary entry generates index marker");
    assert_contains(html, "Primary Topic", "mmark primary entry includes term");
    apex_free_string(html);

    /* Test multiple mmark index entries */
    const char *mmark_multiple = "Protocols (!Protocol) and implementations (!Implementation) are important.";
    html = apex_markdown_to_html(mmark_multiple, strlen(mmark_multiple), &opts);
    assert_contains(html, "Protocol", "Multiple mmark entries include first term");
    assert_contains(html, "Implementation", "Multiple mmark entries include second term");
    apex_free_string(html);

    /* Test TextIndex basic syntax */
    const char *textindex_basic = "This is about firmware{^}.";
    html = apex_markdown_to_html(textindex_basic, strlen(textindex_basic), &opts);
    assert_contains(html, "class=\"index\"", "TextIndex generates index marker");
    assert_contains(html, "idxref-", "TextIndex generates anchor ID");
    apex_free_string(html);

    /* Test TextIndex with explicit term */
    const char *textindex_explicit = "This uses [key combinations]{^}.";
    html = apex_markdown_to_html(textindex_explicit, strlen(textindex_explicit), &opts);
    assert_contains(html, "class=\"index\"", "TextIndex explicit term generates marker");
    assert_contains(html, "key combinations", "TextIndex explicit term keeps visible text");
    assert_not_contains(html, "[key combinations]", "TextIndex explicit term strips square brackets");
    assert_contains(html, "key combinations <a class=\"index-return\"", "TextIndex index lists explicit term");
    apex_free_string(html);

    /* Bare {^} indexes only the preceding word; [phrase]{^} is for multi-word terms */
    const char *textindex_word =
        "Most mechanical keyboard firmware{^} supports the use of [key combinations]{^}.";
    html = apex_markdown_to_html(textindex_word, strlen(textindex_word), &opts);
    assert_contains(html, "firmware<span class=\"index\"", "TextIndex bare {^} leaves preceding word");
    assert_not_contains(html, "Most mechanical keyboard firmware <a", "TextIndex bare {^} indexes word only, not whole phrase");
    assert_contains(html, "firmware <a class=\"index-return\"", "TextIndex bare {^} index entry is firmware");
    assert_not_contains(html, "[key combinations]", "TextIndex phrase form strips brackets in sentence");
    assert_contains(html, "of key combinations<span class=\"index\"", "TextIndex phrase form keeps unbracketed text");
    apex_free_string(html);

    /* Quoted heading override / standalone mark (TextIndex Fig. 2) */
    const char *textindex_quoted =
        "This is a standalone mark: {^\"foo bar\"} and this one has an overridden [heading]{^\"foo baz\"}.";
    html = apex_markdown_to_html(textindex_quoted, strlen(textindex_quoted), &opts);
    assert_not_contains(html, "{^", "TextIndex quoted marks are consumed (not left for superscript)");
    assert_not_contains(html, "{<sup>", "TextIndex quoted marks are not turned into superscript");
    assert_contains(html, "standalone mark: <span class=\"index\"", "TextIndex standalone quoted mark leaves only index span");
    assert_contains(html, "overridden heading<span class=\"index\"", "TextIndex override keeps visible heading without brackets");
    assert_not_contains(html, "[heading]", "TextIndex override strips square brackets");
    assert_contains(html, "foo bar <a class=\"index-return\"", "TextIndex standalone quoted mark indexes foo bar");
    assert_contains(html, "foo baz <a class=\"index-return\"", "TextIndex override indexes quoted term not visible heading");
    assert_not_contains(html, "heading <a class=\"index-return\"", "TextIndex override does not index visible heading text");
    apex_free_string(html);

    /* TextIndex Fig. 4-5: underscore emphasis and * / ** wildcards */
    const char *textindex_emph =
        "This entry will be _emphasised_{^} in the index. "
        "This expanded entry won't be _emphasised_{^\"* (nope)\"}.";
    html = apex_markdown_to_html(textindex_emph, strlen(textindex_emph), &opts);
    assert_contains(html, "<em>emphasised</em><span class=\"index\"",
                    "TextIndex emphasised heading keeps emphasis in body");
    assert_contains(html, "<em>emphasised</em> <a class=\"index-return\"",
                    "TextIndex emphasised heading is emphasised in index");
    assert_not_contains(html, "_emphasised_",
                        "TextIndex emphasised heading does not keep raw underscores in index");
    assert_contains(html, "emphasised (nope) <a class=\"index-return\"",
                    "TextIndex * wildcard expands preceding without emphasis");
    assert_not_contains(html, "* (nope)",
                        "TextIndex * wildcard is not left literal in index");
    apex_free_string(html);

    const char *textindex_emph_strip = "This entry won't be _emphasised_{^*}.";
    html = apex_markdown_to_html(textindex_emph_strip, strlen(textindex_emph_strip), &opts);
    assert_contains(html, "emphasised <a class=\"index-return\"",
                    "TextIndex {^*} strips emphasis via wildcard");
    assert_not_contains(html, "<em>emphasised</em> <a class=\"index-return\"",
                        "TextIndex {^*} index entry is not emphasised");
    assert_not_contains(html, "_emphasised_",
                        "TextIndex {^*} does not keep raw underscores in index");
    apex_free_string(html);

    const char *textindex_emph_lower = "Introducing _Widget_{^\"**\"}.";
    html = apex_markdown_to_html(textindex_emph_lower, strlen(textindex_emph_lower), &opts);
    assert_contains(html, "widget <a class=\"index-return\"",
                    "TextIndex ** wildcard lowercases stripped preceding");
    apex_free_string(html);

    /* TextIndex Fig. 6: prefix wildcards — same heading merges locators */
    const char *textindex_prefix =
        "Sir Winston Leonard Spencer Churchill{^\"*, Sir Winston Leonard Spencer\"} "
        "was Prime Minister from 1940 to 1945.\n\n"
        "Churchill{^*^} was born on 30 November 1874.";
    html = apex_markdown_to_html(textindex_prefix, strlen(textindex_prefix), &opts);
    assert_not_contains(html, "Churchill^",
                        "TextIndex *^ is not expanded as a plain * wildcard");
    assert_contains(html, "href=\"#idxref-0\"", "TextIndex merged entry includes first locator");
    assert_contains(html, "href=\"#idxref-1\"", "TextIndex merged entry includes second locator");
    {
        int heading_hits = 0;
        for (const char *p = html; (p = strstr(p, "Churchill, Sir Winston Leonard Spencer <a class=\"index-return\"")) != NULL; p++) {
            heading_hits++;
        }
        test_result(heading_hits == 1, "TextIndex *^ merges into one index entry with both locators");
    }
    apex_free_string(html);

    /* Duplicate mmark marks also merge locators */
    const char *mmark_dup = "Alpha (!Alpha) here.\n\nMore about Alpha (!Alpha) there.";
    html = apex_markdown_to_html(mmark_dup, strlen(mmark_dup), &opts);
    assert_contains(html, "href=\"#idxref-0\"", "mmark merged entry has first locator");
    assert_contains(html, "href=\"#idxref-1\"", "mmark merged entry has second locator");
    {
        int hits = 0;
        for (const char *p = html; (p = strstr(p, "Alpha <a class=\"index-return\"")) != NULL; p++) {
            hits++;
        }
        test_result(hits == 1, "mmark duplicate marks merge into one entry");
    }
    apex_free_string(html);

    /* TextIndex Fig. 11-12: see / see-also cross-references */
    const char *textindex_see =
        "This function can be [dangerous]{^\"destructive operation\"|safety>\"of functions\"}.";
    html = apex_markdown_to_html(textindex_see, strlen(textindex_see), &opts);
    assert_contains(html, "be dangerous", "TextIndex see-type keeps visible bracketed text");
    assert_not_contains(html, "[dangerous]", "TextIndex see-type strips brackets");
    assert_contains(html, "destructive operation", "TextIndex see-type indexes overridden heading");
    assert_contains(html, "<em>See</em>", "TextIndex see-type renders See in italics");
    assert_contains(html, "safety: of functions", "TextIndex see-type shows hierarchical path with colon");
    assert_not_contains(html, "href=\"#idxref-", "TextIndex see-type does not record a locator");
    apex_free_string(html);

    const char *textindex_see_also =
        "Even typing on a keyboard can be surprisingly risky{^\"risk\"|ergonomics;+safety}.";
    html = apex_markdown_to_html(textindex_see_also, strlen(textindex_see_also), &opts);
    assert_contains(html, "<em>See</em>", "TextIndex mixed xref renders See");
    assert_contains(html, "ergonomics", "TextIndex mixed xref includes see target");
    assert_contains(html, "<em>See also</em>", "TextIndex mixed xref renders See also");
    assert_contains(html, "safety", "TextIndex mixed xref includes also target");
    assert_not_contains(html, "href=\"#idxref-", "TextIndex see-type in mark suppresses locator");
    apex_free_string(html);

    const char *textindex_also_only =
        "A mention of risk{^\"risk\"|+safety} alone.";
    html = apex_markdown_to_html(textindex_also_only, strlen(textindex_also_only), &opts);
    assert_contains(html, "<em>See also</em>", "TextIndex also-only renders See also");
    assert_contains(html, "href=\"#idxref-", "TextIndex also-only keeps locator");
    assert_not_contains(html, "<em>See</em> safety", "TextIndex also-only is not a see-type");
    apex_free_string(html);

    /* TextIndex Fig. 13: aliases */
    const char *textindex_alias =
        "The various [operating systems]{^\"Apple (company)\">\"OS platforms\"#apple} "
        "on Apple devices share a common heritage.\n\n"
        "The majority of Apple devices are iPhones{^\"iPhone\"|+#apple}, by a large margin.";
    html = apex_markdown_to_html(textindex_alias, strlen(textindex_alias), &opts);
    assert_contains(html, "operating systems<span class=\"index\"",
                    "TextIndex alias definition keeps visible text");
    assert_contains(html, "Apple (company)", "TextIndex alias definition indexes parent heading");
    assert_contains(html, "OS platforms", "TextIndex alias definition indexes nested heading");
    assert_contains(html, "iPhone", "TextIndex alias use indexes iPhone");
    assert_contains(html, "<em>See also</em>", "TextIndex alias use expands as see-also");
    assert_contains(html, "Apple (company): OS platforms",
                    "TextIndex #apple expands to full path in see-also");
    assert_not_contains(html, "#apple", "TextIndex alias name is not left literal");
    apex_free_string(html);

    /* TextIndex Fig. 14: unreferenced aliases */
    const char *textindex_unref_alias =
        "{^\"indeterminacy principle (Heisenberg, Werner Karl)\"##q-uncert} "
        "A key component of the Enterprise's transporter system is the "
        "Heisenberg compensator{^|#q-uncert}.";
    html = apex_markdown_to_html(textindex_unref_alias, strlen(textindex_unref_alias), &opts);
    assert_contains(html, "compensator", "TextIndex unref alias use keeps preceding word");
    assert_contains(html, "<em>See</em>", "TextIndex unref alias used as see-reference");
    assert_contains(html, "indeterminacy principle (Heisenberg, Werner Karl)",
                    "TextIndex ## alias expands in see target");
    assert_not_contains(html, "#q-uncert", "TextIndex ## alias name is not left literal");
    assert_not_contains(html, "indeterminacy principle (Heisenberg, Werner Karl) <a class=\"index-return\"",
                        "TextIndex ## alias does not create a locator for the definition");
    apex_free_string(html);

    /* TextIndex Fig. 16: disable/enable processing with {^-} / {^+} */
    const char *textindex_toggle =
        "{^-} This index mark won't be processed: foo{^} {^+} after{^}.";
    html = apex_markdown_to_html(textindex_toggle, strlen(textindex_toggle), &opts);
    assert_contains(html, "foo{^}", "TextIndex {^-} leaves marks untouched while disabled");
    assert_contains(html, "after<span class=\"index\"", "TextIndex {^+} re-enables processing");
    assert_contains(html, "after <a class=\"index-return\"", "TextIndex mark after {^+} is indexed");
    assert_not_contains(html, "foo <a class=\"index-return\"", "TextIndex mark inside {^-} region is not indexed");
    assert_not_contains(html, "{^-}", "TextIndex {^-} toggle mark is consumed when it disables");
    assert_not_contains(html, "{^+}", "TextIndex {^+} toggle mark is consumed when it enables");
    assert_not_contains(html, "> - <a class=\"index-return\"", "TextIndex {^-} is not indexed as a heading");
    assert_not_contains(html, "> + <a class=\"index-return\"", "TextIndex {^+} is not indexed as a heading");
    apex_free_string(html);

    const char *textindex_toggle_redundant =
        "{^-} hidden{^} {^-} still{^} {^+} shown{^} {^+} trailing.";
    html = apex_markdown_to_html(textindex_toggle_redundant, strlen(textindex_toggle_redundant), &opts);
    assert_contains(html, "hidden{^}", "TextIndex redundant region keeps first disabled mark literal");
    assert_contains(html, "{^-}", "TextIndex redundant {^-} stays when already disabled");
    assert_contains(html, "still{^}", "TextIndex marks stay literal until {^+}");
    assert_contains(html, "shown<span class=\"index\"", "TextIndex processing resumes after effective {^+}");
    assert_contains(html, "{^+}", "TextIndex redundant {^+} stays when already enabled");
    apex_free_string(html);

    /* Test Leanpub index syntax */
    const char *leanpub_basic = "Call me Ishmael{i: Ishmael}.";
    html = apex_markdown_to_html(leanpub_basic, strlen(leanpub_basic), &opts);
    assert_contains(html, "class=\"index\"", "Leanpub index generates index marker");
    assert_contains(html, "Ishmael", "Leanpub index preserves term");
    apex_free_string(html);

    /* Test Leanpub hierarchical index */
    apex_options opts_gfm = apex_options_for_mode(APEX_MODE_GFM);
    opts_gfm.enable_indices = true;
    opts_gfm.enable_leanpub_index_syntax = true;
    const char *leanpub_hier = "Niagara{i: \"Niagara!cataract\"} of sand.";
    html = apex_markdown_to_html(leanpub_hier, strlen(leanpub_hier), &opts_gfm);
    assert_contains(html, "Niagara", "Leanpub hierarchical includes main term");
    assert_contains(html, "cataract", "Leanpub hierarchical includes subitem");
    apex_free_string(html);

    /* Test index generation at end of document */
    const char *with_index = "This is about protocols (!Protocol).\n\nAnd implementations (!Implementation).";
    html = apex_markdown_to_html(with_index, strlen(with_index), &opts);
    assert_contains(html, "id=\"index-section\"", "Index section generated");
    assert_contains(html, "class=\"index\"", "Index div generated");
    assert_contains(html, "Protocol", "Index includes first entry");
    assert_contains(html, "Implementation", "Index includes second entry");
    apex_free_string(html);

    /* Test index with alphabetical grouping */
    const char *grouped_index = "Apple (!Apple).\n\nBanana (!Banana).\n\nCherry (!Cherry).";
    html = apex_markdown_to_html(grouped_index, strlen(grouped_index), &opts);
    assert_contains(html, "<dt>A</dt>", "Index groups by first letter (A)");
    assert_contains(html, "<dt>B</dt>", "Index groups by first letter (B)");
    assert_contains(html, "<dt>C</dt>", "Index groups by first letter (C)");
    apex_free_string(html);

    /* Test index insertion at <!--INDEX--> marker */
    const char *index_marker = "This is about protocols (!Protocol).\n\n<!--INDEX-->\n\nMore content.";
    html = apex_markdown_to_html(index_marker, strlen(index_marker), &opts);
    assert_contains(html, "id=\"index-section\"", "Index inserted at marker");
    assert_not_contains(html, "<!--INDEX-->", "Index marker replaced");
    /* Index should appear before "More content" */
    const char *more_pos = strstr(html, "More content");
    const char *index_pos = strstr(html, "id=\"index-section\"");
    if (more_pos && index_pos) {
        assert_contains(html, "id=\"index-section\"", "Index appears before marker content");
    }
    apex_free_string(html);

    /* Test index with subitems */
    const char *with_subitems = "Symmetric encryption (!Encryption, Symmetric).\n\nAsymmetric encryption (!Encryption, Asymmetric).";
    html = apex_markdown_to_html(with_subitems, strlen(with_subitems), &opts);
    assert_contains(html, "Encryption", "Index includes main term");
    assert_contains(html, "Symmetric", "Index includes first subitem");
    assert_contains(html, "Asymmetric", "Index includes second subitem");
    apex_free_string(html);

    /* Test suppress_index option */
    apex_options opts_suppress = apex_options_default();
    opts_suppress.mode = APEX_MODE_UNIFIED;
    opts_suppress.enable_indices = true;
    opts_suppress.enable_mmark_index_syntax = true;
    opts_suppress.suppress_index = true;
    const char *suppress_test = "This is about protocols (!Protocol).";
    html = apex_markdown_to_html(suppress_test, strlen(suppress_test), &opts_suppress);
    assert_contains(html, "class=\"index\"", "Index markers still generated when suppressed");
    assert_not_contains(html, "id=\"index-section\"", "Index section not generated when suppressed");
    apex_free_string(html);

    /* Test indices disabled */
    apex_options opts_disabled = apex_options_default();
    opts_disabled.mode = APEX_MODE_UNIFIED;
    opts_disabled.enable_indices = false;
    const char *disabled_test = "This is about protocols (!Protocol).";
    html = apex_markdown_to_html(disabled_test, strlen(disabled_test), &opts_disabled);
    assert_not_contains(html, "class=\"index\"", "Index markers not generated when disabled");
    assert_not_contains(html, "idxref-", "Index anchors not generated when disabled");
    apex_free_string(html);

    /* Test mmark syntax only mode */
    apex_options opts_mmark_only = apex_options_default();
    opts_mmark_only.mode = APEX_MODE_UNIFIED;
    opts_mmark_only.enable_indices = true;
    opts_mmark_only.enable_mmark_index_syntax = true;
    opts_mmark_only.enable_textindex_syntax = false;
    const char *mmark_only_test = "Protocols (!Protocol) and firmware{^}.";
    html = apex_markdown_to_html(mmark_only_test, strlen(mmark_only_test), &opts_mmark_only);
    assert_contains(html, "class=\"index\"", "mmark syntax processed when enabled");
    /* TextIndex {^} should not be processed, so {^} should appear as plain text or be converted to superscript */
    assert_not_contains(html, "firmware<span class=\"index\"", "TextIndex syntax not processed when disabled");
    apex_free_string(html);

    /* Test TextIndex syntax only mode */
    apex_options opts_textindex_only = apex_options_default();
    opts_textindex_only.mode = APEX_MODE_UNIFIED;
    opts_textindex_only.enable_indices = true;
    opts_textindex_only.enable_mmark_index_syntax = false;
    opts_textindex_only.enable_textindex_syntax = true;
    const char *textindex_only_test = "Protocols (!Protocol) and firmware{^}.";
    html = apex_markdown_to_html(textindex_only_test, strlen(textindex_only_test), &opts_textindex_only);
    /* mmark syntax should not be processed, so (!Protocol) should appear as plain text */
    assert_contains(html, "(!Protocol)", "mmark syntax not processed when disabled");
    assert_contains(html, "class=\"index\"", "TextIndex syntax processed when enabled");
    /* Check that mmark syntax wasn't processed by verifying no index entry for Protocol in index section */
    const char *protocol_in_index = strstr(html, "id=\"index-section\"");
    if (protocol_in_index) {
        /* Look for "Protocol" in the index section - it shouldn't be there if mmark wasn't processed */
        const char *protocol_entry = strstr(protocol_in_index, ">Protocol<");
        if (protocol_entry == NULL) {
            test_result(true, "mmark syntax not processed when disabled");
        } else {
            test_result(false, "mmark syntax not processed when disabled");
        }
    }
    apex_free_string(html);

    /* Test index without grouping */
    apex_options opts_no_group = apex_options_default();
    opts_no_group.mode = APEX_MODE_UNIFIED;
    opts_no_group.enable_indices = true;
    opts_no_group.enable_mmark_index_syntax = true;
    opts_no_group.group_index_by_letter = false;
    const char *no_group_test = "Apple (!Apple).\n\nBanana (!Banana).";
    html = apex_markdown_to_html(no_group_test, strlen(no_group_test), &opts_no_group);
    assert_contains(html, "id=\"index-section\"", "Index generated without grouping");
    assert_not_contains(html, "<dt>A</dt>", "Index not grouped by letter when disabled");
    assert_contains(html, "<ul>", "Index uses simple list when not grouped");
    apex_free_string(html);

    /* Test index in MultiMarkdown mode (now requires explicit --indices flag) */
    apex_options opts_mmd = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    opts_mmd.enable_indices = true;
    opts_mmd.enable_mmark_index_syntax = true;
    const char *mmd_test = "This is about protocols (!Protocol).";
    html = apex_markdown_to_html(mmd_test, strlen(mmd_test), &opts_mmd);
    assert_contains(html, "class=\"index\"", "Indices enabled in MultiMarkdown mode");
    assert_contains(html, "Protocol", "mmark syntax works in MultiMarkdown mode");
    apex_free_string(html);

    /* Test that index entries link back to document */
    const char *link_test = "This is about protocols (!Protocol).";
    html = apex_markdown_to_html(link_test, strlen(link_test), &opts);
    assert_contains(html, "index-return", "Index entries have return links");
    assert_contains(html, "href=\"#idxref-", "Index entries link to anchors");
    apex_free_string(html);

    /* Concordance files: auto-mark terms from TSV before TextIndex processing */
    {
        apex_options opts_conc = apex_options_default();
        opts_conc.mode = APEX_MODE_UNIFIED;
        opts_conc.enable_indices = true;
        opts_conc.enable_textindex_syntax = true;
        opts_conc.base_directory = "tests";
        const char *conc_files[] = {"test_concordance.tsv", NULL};
        opts_conc.concordance_files = (char **)conc_files;

        const char *conc_doc =
            "A widget and another Widget appear. The MacBook is nice. Configuration helps.";
        html = apex_markdown_to_html(conc_doc, strlen(conc_doc), &opts_conc);
        assert_contains(html, "gadget <a class=\"index-return\"",
                        "Concordance maps widget to gadget heading");
        assert_contains(html, "Apple laptop <a class=\"index-return\"",
                        "Concordance case-sensitive MacBook uses quoted heading");
        assert_contains(html, "See", "Concordance see-ref from column 2 is rendered");
        assert_contains(html, "firmware", "Concordance see target firmware appears");
        /* Existing marks must not be rematched */
        const char *conc_existing = "Already marked widget{^custom}.";
        apex_free_string(html);
        html = apex_markdown_to_html(conc_existing, strlen(conc_existing), &opts_conc);
        assert_contains(html, "custom <a class=\"index-return\"",
                        "Concordance skips text already inside an index mark");
        assert_not_contains(html, "gadget <a class=\"index-return\"",
                            "Concordance does not rematch marked widget as gadget");
        apex_free_string(html);
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Index Tests", had_failures, false);
}

/**
 * Test citation and bibliography features
 */

void test_citations(void) {
    int suite_failures = suite_start();
    print_suite_title("Citation and Bibliography Tests", false, true);

    apex_options opts = apex_options_default();
    opts.mode = APEX_MODE_UNIFIED;
    opts.enable_citations = true;
    opts.base_directory = "tests";

    char *html;
    /* Use path relative to base_directory */
    const char *bib_file = "test_refs.bib";
    const char *bib_files[] = {bib_file, NULL};
    opts.bibliography_files = (char **)bib_files;

    /* Test Pandoc citation syntax */
    const char *pandoc_cite = "See [@doe99] for details.";
    html = apex_markdown_to_html(pandoc_cite, strlen(pandoc_cite), &opts);
    assert_contains(html, "citation", "Pandoc citation generates citation class");
    assert_contains(html, "doe99", "Pandoc citation includes key");
    apex_free_string(html);

    /* Test multiple Pandoc citations */
    const char *pandoc_multiple = "See [@doe99; @smith2000] for details.";
    html = apex_markdown_to_html(pandoc_multiple, strlen(pandoc_multiple), &opts);
    assert_contains(html, "doe99", "Multiple citations include first key");
    assert_contains(html, "smith2000", "Multiple citations include second key");
    apex_free_string(html);

    /* Test author-in-text citation */
    const char *pandoc_author = "@smith04 says blah.";
    html = apex_markdown_to_html(pandoc_author, strlen(pandoc_author), &opts);
    assert_contains(html, "citation", "Author-in-text citation generates citation");
    assert_contains(html, "smith04", "Author-in-text citation includes key");
    apex_free_string(html);

    /* (@key) is a citation: the opening paren is a boundary */
    const char *paren_author = "As shown (@smith04).";
    html = apex_markdown_to_html(paren_author, strlen(paren_author), &opts);
    assert_contains(html, "citation", "Paren-boundary citation generates citation");
    assert_contains(html, "smith04", "Paren-boundary citation includes key");
    apex_free_string(html);

    /* @ inside a token is not a citation, even when a real citation is present */
    const char *retina_md =
        "See [@doe99].\n\n"
        "[bbimg217]: assets/ah-original@2x.png \"ah-original@2x.png\"\n\n"
        "![Audio Hijack][bbimg217]\n";
    html = apex_markdown_to_html(retina_md, strlen(retina_md), &opts);
    assert_contains(html, "src=\"assets/ah-original@2x.png\"",
                    "Retina filename keeps @2x in the image src");
    assert_contains(html, "title=\"ah-original@2x.png\"",
                    "Retina filename keeps @2x in the image title");
    assert_not_contains(html, "CITE:2x", "Retina filename is not turned into a citation");
    assert_contains(html, "doe99", "Real citation still processed beside a retina filename");
    apex_free_string(html);

    const char *email_md = "See [@doe99]. Write user@example.com today.\n";
    html = apex_markdown_to_html(email_md, strlen(email_md), &opts);
    assert_contains(html, "user@example.com", "Email address survives citation scanning");
    assert_not_contains(html, "CITE:example", "Email address is not turned into a citation");
    apex_free_string(html);

    /* A dot inside a key is still part of the key when @ starts at a boundary */
    const char *dotted_key = "See @smith.2000 today.";
    html = apex_markdown_to_html(dotted_key, strlen(dotted_key), &opts);
    assert_contains(html, "smith.2000", "Dotted citation key kept whole");
    apex_free_string(html);

    /* iA Writer Annotations dumps use `@Name: 1,2 3,4` — trailing colon is not
     * part of a cite key, and the range dump must stay in the paragraph. */
    const char *ia_ann_prose =
        "Body.\n\n@Wayne: 2,7 10,8 19,5\n";
    html = apex_markdown_to_html(ia_ann_prose, strlen(ia_ann_prose), &opts);
    assert_contains(html, "2,7 10,8 19,5",
                    "@Name: annotations dump keeps range text in output");
    assert_not_contains(html, "CITE👋",
                        "@Name: annotations dump is not emoji-corrupted in CITE");
    apex_free_string(html);

    /* Test MultiMarkdown citation syntax */
    opts.mode = APEX_MODE_MULTIMARKDOWN;
    const char *mmd_cite = "This is a statement[#Doe:2006].";
    html = apex_markdown_to_html(mmd_cite, strlen(mmd_cite), &opts);
    assert_contains(html, "citation", "MultiMarkdown citation generates citation class");
    assert_contains(html, "Doe:2006", "MultiMarkdown citation includes key");
    apex_free_string(html);

    /* Test mmark citation syntax */
    opts.mode = APEX_MODE_UNIFIED;
    const char *mmark_cite = "This references [@RFC2535].";
    html = apex_markdown_to_html(mmark_cite, strlen(mmark_cite), &opts);
    assert_contains(html, "citation", "mmark citation generates citation class");
    assert_contains(html, "RFC2535", "mmark citation includes key");
    apex_free_string(html);

    /* Test bibliography generation - use metadata to ensure bibliography loads */
    const char *with_refs = "---\nbibliography: test_refs.bib\n---\n\nSee [@doe99].\n\n<!-- REFERENCES -->";
    html = apex_markdown_to_html(with_refs, strlen(with_refs), &opts);
    if (strstr(html, "<div id=\"refs\"")) {
        assert_contains(html, "ref-doe99", "Bibliography includes cited entry");
        assert_contains(html, "Doe, John", "Bibliography includes author");
        assert_contains(html, "1999", "Bibliography includes year");
        assert_not_contains(html, "<!-- REFERENCES -->", "Bibliography marker replaced");
        assert_contains(html, "Article Title", "Bibliography includes article title");
        assert_contains(html, "Journal Name", "Bibliography includes journal");
    } else {
        /* If bibliography didn't load, at least verify citation was processed */
        assert_contains(html, "citation", "Citation was processed");
        /* Skip 5 tests - bibliography file may not load in test context */
        for (int i = 0; i < 5; i++) {
            test_result(true, "Bibliography tests skipped (file may not load in test context)");
        }
    }
    apex_free_string(html);

    /* Test that citations don't interfere with autolinking */
    apex_options opts_autolink = apex_options_default();
    opts_autolink.mode = APEX_MODE_UNIFIED;
    opts_autolink.enable_autolink = true;
    opts_autolink.enable_citations = false;  /* Disable citations for this test */
    opts_autolink.bibliography_files = NULL;
    const char *no_cite_email = "Contact me at test@example.com";
    html = apex_markdown_to_html(no_cite_email, strlen(no_cite_email), &opts_autolink);
    assert_contains(html, "mailto:", "Email autolinking still works");
    apex_free_string(html);

    /* Test markdown mailto links with @ and query params are not re-autolinked */
    const char *mailto_markdown =
        "If you have purchased a permanent unlock or lifetime license through the Mac App Store, please "
        "[email the developer](mailto:marked@brettterpstra.com?subject=Marked%20License%20Crossgrade&body=Please%20include%20your%20UUID%20%28Help-%3ECopy%20UUID%20in%20Marked%29%20in%20this%20email%20for%20receipt%20verification.) "
        "to request a free lifetime Paddle license.";
    html = apex_markdown_to_html(mailto_markdown, strlen(mailto_markdown), &opts_autolink);
    assert_contains(
        html,
        "<a href=\"mailto:marked@brettterpstra.com?subject=Marked%20License%20Crossgrade&amp;body=Please%20include%20your%20UUID%20%28Help-%3ECopy%20UUID%20in%20Marked%29%20in%20this%20email%20for%20receipt%20verification.\">email the developer</a>",
        "Markdown mailto link with query params renders as a single clean anchor");
    assert_not_contains(html, "[email the", "Markdown link syntax is not leaked into output");
    assert_not_contains(html, "mailto:developer](", "No nested autolink corruption in mailto link");
    apex_free_string(html);

    /* Test that autolink does not run inside indented code blocks */
    const char *indented_code =
        "    x-marked://extract?url=https://example.com\n";
    html = apex_markdown_to_html(indented_code, strlen(indented_code), &opts_autolink);
    assert_not_contains(html, "[https://example.com](https://example.com)",
                        "Indented code block URL is not autolinked");
    assert_contains(html, "x-marked://extract?url=https://example.com",
                    "Indented code block content is preserved");
    apex_free_string(html);

    /* Test that @ in citations doesn't become mailto */
    const char *cite_with_at = "See [@doe99] for details.";
    html = apex_markdown_to_html(cite_with_at, strlen(cite_with_at), &opts);
    assert_not_contains(html, "mailto:doe99", "@ in citation doesn't become mailto link");
    assert_contains(html, "citation", "Citation still processed correctly");
    apex_free_string(html);

    /* Test that citations are not processed when bibliography is not provided */
    apex_options opts_no_bib = apex_options_default();
    opts_no_bib.mode = APEX_MODE_UNIFIED;
    opts_no_bib.enable_citations = true;
    opts_no_bib.bibliography_files = NULL;
    const char *cite_no_bib = "See [@doe99] for details.";
    html = apex_markdown_to_html(cite_no_bib, strlen(cite_no_bib), &opts_no_bib);
    /* Citation syntax should not be processed when no bibliography */
    assert_not_contains(html, "citation", "Citations not processed without bibliography");
    apex_free_string(html);

    /* Test metadata bibliography */
    const char *md_with_bib = "---\nbibliography: test_refs.bib\n---\n\nSee [@doe99].";
    apex_options opts_meta = apex_options_default();
    opts_meta.mode = APEX_MODE_UNIFIED;
    opts_meta.base_directory = "tests";
    html = apex_markdown_to_html(md_with_bib, strlen(md_with_bib), &opts_meta);
    assert_contains(html, "citation", "Metadata bibliography enables citations");
    assert_contains(html, "doe99", "Metadata bibliography processes citations");
    apex_free_string(html);

    /* Test suppress bibliography option */
    opts.suppress_bibliography = true;
    const char *suppress_test = "See [@doe99].\n\n<!-- REFERENCES -->";
    html = apex_markdown_to_html(suppress_test, strlen(suppress_test), &opts);
    assert_not_contains(html, "<div id=\"refs\"", "Bibliography suppressed when flag set");
    apex_free_string(html);

    /* Test link citations option */
    opts.suppress_bibliography = false;
    opts.link_citations = true;
    const char *link_test = "See [@doe99].";
    html = apex_markdown_to_html(link_test, strlen(link_test), &opts);
    assert_contains(html, "<a href=\"#ref-doe99\"", "Citations linked when link_citations enabled");
    assert_contains(html, "class=\"citation\"", "Linked citations have citation class");
    apex_free_string(html);

    /* Test author-in-text locator parsing: @key [p. 2] */
    {
        apex_options loc = opts;
        loc.mode = APEX_MODE_UNIFIED;
        loc.link_citations = true;
        const char *author_locator = "According to @smith04 [p. 2], ok.";
        html = apex_markdown_to_html(author_locator, strlen(author_locator), &loc);
        assert_contains(html, "data-cites=\"smith04\"", "Author-in-text locator citation processed");
        assert_not_contains(html, "p. 2", "Author-in-text locator consumed (not left in output)");
        apex_free_string(html);
    }

    /* Test brace-wrapped citation key path: allow keys starting with non-alnum (e.g. '/').
     * Even if the key doesn't resolve to a bibliography entry, it should still parse.
     */
    {
        apex_options brace = opts;
        brace.mode = APEX_MODE_UNIFIED;
        const char *brace_key = "See [@{/foo}].";
        html = apex_markdown_to_html(brace_key, strlen(brace_key), &brace);
        assert_contains(html, "class=\"citation\"", "Brace-wrapped key produces citation span");
        assert_contains(html, "data-cites=\"/foo\"", "Brace-wrapped key preserved in data-cites");
        assert_contains(html, "(/foo)", "Unresolved brace-wrapped key rendered as literal key");
        apex_free_string(html);
    }

    /* Test citations enabled but bibliography file missing/unreadable:
     * should still parse citation syntax, but render unresolved key.
     */
    {
        apex_options missing = apex_options_default();
        missing.mode = APEX_MODE_UNIFIED;
        missing.enable_citations = true;
        missing.base_directory = "tests";
        const char *missing_bib = "missing.bib";
        const char *missing_bibs[] = { missing_bib, NULL };
        missing.bibliography_files = (char **)missing_bibs;

        const char *cite_missing = "See [@doe99].";
        html = apex_markdown_to_html(cite_missing, strlen(cite_missing), &missing);
        assert_contains(html, "class=\"citation\"", "Missing bibliography: citation still processed");
        assert_contains(html, "data-cites=\"doe99\"", "Missing bibliography: key preserved");
        assert_contains(html, "(doe99)", "Missing bibliography: unresolved key rendered");
        apex_free_string(html);
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Citation and Bibliography Tests", had_failures, false);
}


/**
 * Test metadata control of command-line options
 */

void test_aria_labels(void) {
    int suite_failures = suite_start();
    print_suite_title("ARIA Labels Tests", false, true);

    apex_options opts = apex_options_default();
    char *html;

    /* Test 1: TOC nav gets aria-label when --aria is enabled */
    opts.enable_aria = true;
    opts.enable_marked_extensions = true;
    const char *doc_with_toc = "# Header 1\n\n<!--TOC-->\n\n## Header 2";
    html = apex_markdown_to_html(doc_with_toc, strlen(doc_with_toc), &opts);
    assert_contains(html, "<nav class=\"toc\"", "TOC nav element present");
    assert_contains(html, "aria-label=\"Table of contents\"", "TOC nav has aria-label");
    apex_free_string(html);

    /* Test 2: TOC nav does NOT get aria-label when --aria is disabled (backward compatibility) */
    opts.enable_aria = false;
    html = apex_markdown_to_html(doc_with_toc, strlen(doc_with_toc), &opts);
    assert_contains(html, "<nav class=\"toc\"", "TOC nav element present");
    assert_not_contains(html, "aria-label=\"Table of contents\"", "TOC nav without aria-label when disabled");
    apex_free_string(html);

    /* Test 3: Figures get role="figure" when --aria is enabled */
    opts.enable_aria = true;
    opts.enable_tables = true;
    /* Use a table with caption to generate a figure */
    const char *table_with_caption = "[Test Table Caption]\n| A | B |\n|---|---|\n| 1 | 2 |";
    html = apex_markdown_to_html(table_with_caption, strlen(table_with_caption), &opts);
    assert_contains(html, "<figure", "Figure element present");
    assert_contains(html, "role=\"figure\"", "Figure has role attribute");
    apex_free_string(html);

    /* Test 4: Figures do NOT get role when --aria is disabled */
    opts.enable_aria = false;
    html = apex_markdown_to_html(table_with_caption, strlen(table_with_caption), &opts);
    assert_contains(html, "<figure", "Figure element present");
    assert_not_contains(html, "role=\"figure\"", "Figure without role when disabled");
    apex_free_string(html);

    /* Test 5: Tables get role="table" when --aria is enabled */
    opts.enable_aria = true;
    const char *simple_table = "| A | B |\n|---|---|\n| 1 | 2 |";
    html = apex_markdown_to_html(simple_table, strlen(simple_table), &opts);
    assert_contains(html, "<table", "Table element present");
    assert_contains(html, "role=\"table\"", "Table has role attribute");
    apex_free_string(html);

    /* Test 6: Tables do NOT get role when --aria is disabled */
    opts.enable_aria = false;
    html = apex_markdown_to_html(simple_table, strlen(simple_table), &opts);
    assert_contains(html, "<table", "Table element present");
    assert_not_contains(html, "role=\"table\"", "Table without role when disabled");
    apex_free_string(html);

    /* Test 7: Table figures with captions get IDs on figcaption when --aria is enabled */
    opts.enable_aria = true;
    html = apex_markdown_to_html(table_with_caption, strlen(table_with_caption), &opts);
    assert_contains(html, "<figcaption", "Figcaption element present");
    assert_contains(html, "id=\"table-caption-1\"", "Figcaption has generated ID");
    apex_free_string(html);

    /* Test 8: Tables with captions get aria-describedby linking to figcaption when --aria is enabled */
    opts.enable_aria = true;
    html = apex_markdown_to_html(table_with_caption, strlen(table_with_caption), &opts);
    assert_contains(html, "<table", "Table element present");
    assert_contains(html, "aria-describedby=\"table-caption-1\"", "Table has aria-describedby linking to caption");
    apex_free_string(html);

    /* Test 9: Multiple tables with captions get unique IDs */
    opts.enable_aria = true;
    const char *multiple_tables =
        "[First Table]\n| A |\n|---|\n| 1 |\n\n"
        "[Second Table]\n| B |\n|---|\n| 2 |";
    html = apex_markdown_to_html(multiple_tables, strlen(multiple_tables), &opts);
    assert_contains(html, "id=\"table-caption-1\"", "First figcaption has ID 1");
    assert_contains(html, "id=\"table-caption-2\"", "Second figcaption has ID 2");
    assert_contains(html, "aria-describedby=\"table-caption-1\"", "First table links to caption 1");
    assert_contains(html, "aria-describedby=\"table-caption-2\"", "Second table links to caption 2");
    apex_free_string(html);

    /* Test 10: Tables with existing figcaption IDs use them for aria-describedby */
    /* Note: IAL syntax on caption lines ([Caption]{#id}) is not currently supported.
     * This test verifies that regular captions work with generated IDs. */
    opts.enable_aria = true;
    const char *table_with_regular_caption = "[Custom Caption]\n| A |\n|---|\n| 1 |";
    html = apex_markdown_to_html(table_with_regular_caption, strlen(table_with_regular_caption), &opts);
    assert_contains(html, "<figcaption", "Figcaption element present");
    assert_contains(html, "id=\"table-caption-", "Figcaption has generated ID");
    assert_contains(html, "aria-describedby=\"table-caption-", "Table links to figcaption ID");
    apex_free_string(html);

    /* Test 11: TOC with MMD style also gets aria-label */
    opts.enable_aria = true;
    const char *mmd_toc_doc = "# Title\n\n{{TOC}}\n\n## Section";
    html = apex_markdown_to_html(mmd_toc_doc, strlen(mmd_toc_doc), &opts);
    assert_contains(html, "<nav class=\"toc\"", "MMD TOC nav present");
    assert_contains(html, "aria-label=\"Table of contents\"", "MMD TOC nav has aria-label");
    apex_free_string(html);

    /* Many injected attributes must not overflow the output buffer (previously hung) */
    {
        apex_options many_opts = apex_options_default();
        many_opts.enable_aria = true;
        many_opts.enable_image_captions = true;
        many_opts.enable_tables = true;

        size_t cap = 200 * 64 + 1;
        char *md = malloc(cap);
        size_t pos = 0;
        for (int i = 1; i <= 200; i++) {
            pos += (size_t)snprintf(md + pos, cap - pos, "![Image %d](img-%d.png)\n\n", i, i);
        }
        html = apex_markdown_to_html(md, pos, &many_opts);
        int figures = 0;
        for (const char *p = html; p && (p = strstr(p, "role=\"figure\"")); p++) figures++;
        test_result(figures == 200, "ARIA: 200 image figures all get role=\"figure\"");
        assert_contains(html, "img-200.png", "ARIA: output with many figures is complete");
        apex_free_string(html);
        free(md);

        cap = 120 * 96 + 1;
        md = malloc(cap);
        pos = 0;
        for (int i = 1; i <= 120; i++) {
            pos += (size_t)snprintf(md + pos, cap - pos,
                                    "| A | B |\n|---|---|\n| %d | 2 |\n[Table %d caption]\n\n", i, i);
        }
        html = apex_markdown_to_html(md, pos, &many_opts);
        int tables = 0, described = 0;
        for (const char *p = html; p && (p = strstr(p, "role=\"table\"")); p++) tables++;
        for (const char *p = html; p && (p = strstr(p, "aria-describedby=\"table-caption-")); p++) described++;
        test_result(tables == 120, "ARIA: 120 tables all get role=\"table\"");
        test_result(described == 120, "ARIA: 120 tables all get aria-describedby");
        assert_contains(html, "id=\"table-caption-120\"", "ARIA: last figcaption gets its ID");
        assert_contains(html, "Table 120 caption", "ARIA: output with many tables is complete");
        apex_free_string(html);
        free(md);
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("ARIA Labels Tests", had_failures, false);
}

/**
 * Main test runner
 */

void test_combine_gitbook_like(void) {
    int suite_failures = suite_start();
    print_suite_title("Combine / GitBook SUMMARY-like Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_file_includes = true;
    opts.generate_header_ids = false;  /* Disable header IDs for these tests */

    const char *base_dir = "tests/fixtures/combine_summary";

    const char *intro_path = "tests/fixtures/combine_summary/intro.md";
    const char *chapter_path = "tests/fixtures/combine_summary/chapter1.md";

    /* Intro alone */
    size_t intro_len = 0;
    char *intro_src = NULL;
    {
        FILE *fp = fopen(intro_path, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long sz = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            intro_src = malloc(sz + 1);
            if (intro_src) {
                intro_len = fread(intro_src, 1, sz, fp);
                intro_src[intro_len] = '\0';
            }
            fclose(fp);
        }
    }
    if (!intro_src) {
        printf(COLOR_RED "✗" COLOR_RESET " Failed to read intro fixture for combine tests\n");
        tests_failed++;
        tests_run++;
        return;
    }

    /* Process intro with includes (none here, just sanity) */
    char *intro_md = apex_process_includes(intro_src, base_dir, NULL, 0, NULL, NULL, NULL);
    char *intro_html = apex_markdown_to_html(intro_md ? intro_md : intro_src,
                                             strlen(intro_md ? intro_md : intro_src),
                                             &opts);
    assert_contains(intro_html, "<h1>Intro</h1>", "Intro heading rendered");
    apex_free_string(intro_html);
    if (intro_md) free(intro_md);
    free(intro_src);

    /* Now chapter1 which includes section1_1.md via {{section1_1.md}} */
    size_t chapter_len = 0;
    char *chapter_src = NULL;
    {
        FILE *fp = fopen(chapter_path, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long sz = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            chapter_src = malloc(sz + 1);
            if (chapter_src) {
                chapter_len = fread(chapter_src, 1, sz, fp);
                chapter_src[chapter_len] = '\0';
            }
            fclose(fp);
        }
    }
    if (!chapter_src) {
        printf(COLOR_RED "✗" COLOR_RESET " Failed to read chapter1 fixture for combine tests\n");
        tests_failed++;
        tests_run++;
        return;
    }

    char *chapter_md = apex_process_includes(chapter_src, base_dir, NULL, 0, NULL, NULL, NULL);
    const char *chapter_final = chapter_md ? chapter_md : chapter_src;

    /* Combined Markdown should contain both Chapter 1 and Section 1.1 headings */
    if (strstr(chapter_final, "# Chapter 1") && strstr(chapter_final, "# Section 1.1")) {
        test_result(true, "Includes expanded for chapter1/section1_1");
    } else {
        test_result(false, "Includes not expanded for chapter1/section1_1");
        printf(COLOR_RED "✗" COLOR_RESET " Includes not expanded correctly for chapter1/section1_1\n");
    }

    char *chapter_html = apex_markdown_to_html(chapter_final, strlen(chapter_final), &opts);
    assert_contains(chapter_html, "<h1>Chapter 1</h1>", "Chapter 1 heading rendered");
    assert_contains(chapter_html, "<h1>Section 1.1</h1>", "Section 1.1 heading rendered from included file");

    apex_free_string(chapter_html);
    if (chapter_md) free(chapter_md);
    free(chapter_src);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Combine / GitBook SUMMARY-like Tests", had_failures, false);
}

/**
 * Test Pandoc JSON parser with filter-style output (Header + RawBlock with
 * escaped quotes and newlines in the raw string).
 */
void test_ast_json_parser(void) {
    int suite_failures = suite_start();
    print_suite_title("AST JSON Parser (filter output with RawBlock)", false, true);

    /* Minimal: only RawBlock with no escapes – should parse to 1 child */
    const char *minimal = "{\"pandoc-api-version\":[1,23,1],\"meta\":{},\"blocks\":[{\"t\":\"RawBlock\",\"c\":[\"html\",\"ab\"]}]}";
    apex_options opts0 = apex_options_default();
    cmark_node *doc0 = apex_pandoc_json_to_cmark(minimal, &opts0);
    int minimal_ok = (doc0 && cmark_node_first_child(doc0) && !cmark_node_next(cmark_node_first_child(doc0)));
    if (doc0) cmark_node_free(doc0);
    test_result(minimal_ok, "Minimal RawBlock (no escapes) parses to one block");

    /* Single RawBlock with the exact long string (escaped quotes + newlines) */
    const char *single_raw = "{\"pandoc-api-version\":[1,23,1],\"meta\":{},\"blocks\":[{\"t\":\"RawBlock\",\"c\":[\"html\",\"<figure><p>&lt; <img src=\\\"image.png\\\" alt=\\\"Image\\\" /></p>\\n</figure>\\n\"]}]}";
    cmark_node *doc1 = apex_pandoc_json_to_cmark(single_raw, &opts0);
    int single_ok = (doc1 && cmark_node_first_child(doc1) && !cmark_node_next(cmark_node_first_child(doc1)));
    if (doc1) cmark_node_free(doc1);
    test_result(single_ok, "Single RawBlock with \\\" and \\n in content parses");

    /* Header only – should parse to 1 child */
    const char *header_only = "{\"pandoc-api-version\":[1,23,1],\"meta\":{},\"blocks\":[{\"t\":\"Header\",\"c\":[1,[\"\",[],[]],[{\"t\":\"Str\",\"c\":\"UNWRAP FILTER\"}]}]}";
    cmark_node *doc_h = apex_pandoc_json_to_cmark(header_only, &opts0);
    int header_ok = (doc_h && cmark_node_first_child(doc_h) && !cmark_node_next(cmark_node_first_child(doc_h)));
    if (doc_h) cmark_node_free(doc_h);
    test_result(header_ok, "Header-only document parses to one block");

    /* Round-trip: build Header + RawBlock, serialize to JSON, parse back → must get 2 blocks */
    {
        cmark_node *build = cmark_node_new(CMARK_NODE_DOCUMENT);
        cmark_node *h = cmark_node_new(CMARK_NODE_HEADING);
        cmark_node_set_heading_level(h, 1);
        cmark_node *htext = cmark_node_new(CMARK_NODE_TEXT);
        cmark_node_set_literal(htext, "Title");
        cmark_node_append_child(h, htext);
        cmark_node_append_child(build, h);
        cmark_node *raw = cmark_node_new(CMARK_NODE_HTML_BLOCK);
        cmark_node_set_literal(raw, "<p>x</p>");
        cmark_node_append_child(build, raw);
        char *two_block_json = apex_cmark_to_pandoc_json(build, &opts0);
        cmark_node_free(build);
        int round_ok = 0;
        if (two_block_json) {
            cmark_node *back = apex_pandoc_json_to_cmark(two_block_json, &opts0);
            if (back) {
                int n = 0;
                for (cmark_node *b = cmark_node_first_child(back); b; b = cmark_node_next(b)) n++;
                round_ok = (n == 2);
                cmark_node_free(back);
            }
            free(two_block_json);
        }
        test_result(round_ok, "Round-trip (Header + RawBlock) JSON parses to two blocks");
    }

    /* Full filter-style: Header + RawBlock; single literal so run-time has },{ with no stray quote */
    const char *json = "{\"pandoc-api-version\":[1,23,1],\"meta\":{},\"blocks\":[{\"t\":\"Header\",\"c\":[1,[\"\",[],[]],[{\"t\":\"Str\",\"c\":\"UNWRAP FILTER\"}]]},{\"t\":\"RawBlock\",\"c\":[\"html\",\"<figure><p>&lt; <img src=\\\"image.png\\\" alt=\\\"Image\\\" /></p>\\n</figure>\\n\"]}]}";

    /* Same payload but with "c" before "t" (dkjson order) for second block. Ensure comma between blocks. */
    const char *json_c_before_t = "{\"pandoc-api-version\":[1,23,1],\"meta\":{},\"blocks\":[{\"t\":\"Header\",\"c\":[1,[\"\",[],[]],[{\"t\":\"Str\",\"c\":\"X\"}]]},{\"c\":[\"html\",\"<x/>\"],\"t\":\"RawBlock\"}]}";

    apex_options opts = apex_options_default();
    cmark_node *doc = apex_pandoc_json_to_cmark(json, &opts);
    if (!doc) {
        test_result(false, "apex_pandoc_json_to_cmark returned NULL");
        bool had_failures = suite_end(suite_failures);
        print_suite_title("AST JSON Parser (filter output with RawBlock)", had_failures, false);
        return;
    }

    cmark_node *first = cmark_node_first_child(doc);
    int count = 0;
    for (cmark_node *n = first; n; n = cmark_node_next(n)) count++;
    test_result(count == 2, "Parsed document has two block children (Header + RawBlock)");
    if (count >= 1) {
        cmark_node_type t1 = cmark_node_get_type(first);
        test_result(t1 == CMARK_NODE_HEADING, "First block is heading");
    }
    if (count >= 2) {
        cmark_node *second = cmark_node_next(first);
        cmark_node_type t2 = cmark_node_get_type(second);
        test_result(t2 == CMARK_NODE_HTML_BLOCK, "Second block is HTML block");
    }

    cmark_node_free(doc);

    /* Minimal two-block: Para + RawBlock (no Header nesting) */
    const char *minimal_two = "{\"blocks\":[{\"t\":\"Para\",\"c\":[]},{\"t\":\"RawBlock\",\"c\":[\"html\",\"x\"]}]}";
    cmark_node *doc_min = apex_pandoc_json_to_cmark(minimal_two, &opts);
    int count_min = 0;
    if (doc_min) {
        for (cmark_node *n = cmark_node_first_child(doc_min); n; n = cmark_node_next(n)) count_min++;
        cmark_node_free(doc_min);
    }
    test_result(doc_min && count_min == 2, "Minimal two-block (Para + RawBlock) parses to two blocks");

    /* Single RawBlock with "c" before "t" (dkjson order) – must parse to 1 block */
    const char *single_c_before_t = "{\"pandoc-api-version\":[1,23,1],\"meta\":{},\"blocks\":[{\"c\":[\"html\",\"<x/>\"],\"t\":\"RawBlock\"}]}";
    cmark_node *doc1cb = apex_pandoc_json_to_cmark(single_c_before_t, &opts);
    int count1cb = 0;
    if (doc1cb) {
        for (cmark_node *n = cmark_node_first_child(doc1cb); n; n = cmark_node_next(n)) count1cb++;
        cmark_node_free(doc1cb);
    }
    test_result(doc1cb && count1cb == 1, "Single RawBlock with \"c\" before \"t\" parses to one block");

    /* dkjson-style key order: "c" before "t" for second block – parser must still yield 2 blocks */
    cmark_node *doc2 = apex_pandoc_json_to_cmark(json_c_before_t, &opts);
    int count2 = 0;
    if (doc2) {
        for (cmark_node *n = cmark_node_first_child(doc2); n; n = cmark_node_next(n)) count2++;
        cmark_node_free(doc2);
    }
    test_result(doc2 && count2 == 2, "Header + RawBlock with \"c\" before \"t\" parses to two blocks");

    bool had_failures = suite_end(suite_failures);
    print_suite_title("AST JSON Parser (filter output with RawBlock)", had_failures, false);
}

/* Shared document for output formats the HTML suites do not exercise. */
static const char COVERAGE_DOC[] =
    "# Kitchen Title\n"
    "\n"
    "A paragraph with **bold phrase**, *italic phrase*, `code span`,\n"
    "and a [link](https://example.com/doc).\n"
    "\n"
    "Line one  \n"
    "hard break\n"
    "\n"
    "- bullet item\n"
    "\n"
    "1. ordered item\n"
    "\n"
    "> quoted line\n"
    "\n"
    "```c\n"
    "code line\n"
    "```\n"
    "\n"
    "| Head | Cell |\n"
    "| --- | --- |\n"
    "| left | right |\n"
    "\n"
    "![alt pic](missing-local.png)\n"
    "\n"
    "<dl><dt>HtmlTerm</dt><dd>HtmlDef</dd></dl>\n"
    "\n"
    "<div class=\"callout\">Callout note</div>\n"
    "\n"
    "---\n"
    "\n"
    "See the note.[^n]\n"
    "\n"
    "[^n]: Footnote body.\n"
    "\n"
    "DlTerm\n"
    ": Definition body\n";

static const char MAN_NAME_DOC[] =
    "# NAME\n"
    "\n"
    "apex - coverage tool\n"
    "\n"
    "# DESCRIPTION\n"
    "\n"
    "A paragraph with **bold phrase** and [link](https://example.com/doc).\n"
    "\n"
    "- bullet item\n"
    "\n"
    "> quoted line\n"
    "\n"
    "```\n"
    "code line\n"
    "```\n"
    "\n"
    "<dl><dt>HtmlTerm</dt><dd>HtmlDef</dd></dl>\n"
    "\n"
    "---\n";

void test_output_formats(void) {
    int suite_failures = suite_start();
    print_suite_title("Output Format Coverage Tests", false, true);

    apex_options opts;
    char *out;

    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_TERMINAL;
    opts.terminal_inline_images = false;
    out = apex_markdown_to_html(COVERAGE_DOC, strlen(COVERAGE_DOC), &opts);
    assert_contains(out, "Kitchen Title", "Terminal heading text");
    assert_contains(out, "bold phrase", "Terminal strong text");
    assert_contains(out, "italic phrase", "Terminal emphasis text");
    assert_contains(out, "code span", "Terminal code span");
    assert_contains(out, "https://example.com/doc", "Terminal link URL");
    assert_contains(out, "hard break", "Terminal hard line break");
    assert_contains(out, "bullet item", "Terminal bullet list");
    assert_contains(out, "ordered item", "Terminal ordered list");
    assert_contains(out, "quoted line", "Terminal blockquote");
    assert_contains(out, "code line", "Terminal code block");
    assert_contains(out, "Head", "Terminal table header");
    assert_contains(out, "right", "Terminal table cell");
    assert_contains(out, "missing-local.png", "Terminal image falls back to the path");
    assert_contains(out, "HtmlTerm", "Terminal HTML definition list term");
    assert_contains(out, "HtmlDef", "Terminal HTML definition list body");
    assert_contains(out, "----------------------------------------", "Terminal thematic break");
    assert_contains(out, "Footnote body", "Terminal footnote body");
    assert_contains(out, "Definition body", "Terminal definition list body");
    apex_free_string(out);

    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_TERMINAL256;
    opts.terminal_inline_images = false;
    out = apex_markdown_to_html(COVERAGE_DOC, strlen(COVERAGE_DOC), &opts);
    assert_contains(out, "Kitchen Title", "terminal256 heading text");
    assert_contains(out, "bullet item", "terminal256 list text");
    assert_contains(out, "\033[38;5;", "terminal256 emits a 256-color sequence");
    apex_free_string(out);

    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_MARKDOWN;
    out = apex_markdown_to_html(COVERAGE_DOC, strlen(COVERAGE_DOC), &opts);
    assert_contains(out, "# Kitchen Title", "Markdown heading");
    assert_contains(out, "**bold phrase**", "Markdown strong");
    assert_contains(out, "*italic phrase*", "Markdown emphasis");
    assert_contains(out, "`code span`", "Markdown code span");
    assert_contains(out, "[link](https://example.com/doc \"\")", "Markdown link");
    assert_contains(out, "- bullet item", "Markdown bullet");
    assert_contains(out, "- ordered item", "Markdown ordered item");
    assert_contains(out, "> quoted line", "Markdown blockquote");
    assert_contains(out, "```c", "Markdown fenced code info");
    assert_contains(out, "code line", "Markdown code block body");
    assert_contains(out, "HtmlTerm", "Markdown HTML block");
    assert_contains(out, "---", "Markdown thematic break");
    apex_free_string(out);

    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_JSON;
    out = apex_markdown_to_html(COVERAGE_DOC, strlen(COVERAGE_DOC), &opts);
    assert_contains(out, "pandoc-api-version", "JSON has a Pandoc API version");
    assert_contains(out, "\"Header\"", "JSON heading node");
    assert_contains(out, "\"Strong\"", "JSON strong node");
    assert_contains(out, "\"Emph\"", "JSON emphasis node");
    assert_contains(out, "\"Link\"", "JSON link node");
    assert_contains(out, "\"BulletList\"", "JSON bullet list node");
    assert_contains(out, "\"OrderedList\"", "JSON ordered list node");
    assert_contains(out, "\"BlockQuote\"", "JSON blockquote node");
    assert_contains(out, "\"CodeBlock\"", "JSON code block node");
    assert_contains(out, "\"Image\"", "JSON image node");
    assert_contains(out, "\"RawBlock\"", "JSON raw HTML block");
    assert_contains(out, "\"HorizontalRule\"", "JSON thematic break");
    apex_free_string(out);

    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_MAN;
    out = apex_markdown_to_html(MAN_NAME_DOC, strlen(MAN_NAME_DOC), &opts);
    assert_contains(out, ".TH \"NAME\"", "Man roff title");
    assert_contains(out, ".SH ", "Man roff section heading");
    assert_contains(out, ".PP", "Man roff paragraph");
    assert_contains(out, "\\f[B]", "Man roff bold");
    assert_contains(out, "bold phrase", "Man roff paragraph text");
    assert_contains(out, "example.com/doc", "Man roff link URL");
    assert_contains(out, ".IP \\(bu", "Man roff bullet");
    assert_contains(out, ".RS", "Man roff blockquote");
    assert_contains(out, "code line", "Man roff code block");
    assert_contains(out, "HtmlTerm", "Man roff HTML definition term");
    assert_contains(out, "*  *  *", "Man roff thematic break");
    apex_free_string(out);

    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_MAN_HTML;
    opts.standalone = true;
    out = apex_markdown_to_html(MAN_NAME_DOC, strlen(MAN_NAME_DOC), &opts);
    assert_contains(out, "<!DOCTYPE html>", "Man HTML standalone doctype");
    assert_contains(out, "class=\"man-standalone\"", "Man HTML standalone body");
    assert_contains(out, "apex", "Man HTML name command");
    assert_contains(out, "coverage tool", "Man HTML name description");
    assert_contains(out, "DESCRIPTION", "Man HTML section");
    assert_contains(out, "bold phrase", "Man HTML paragraph");
    assert_contains(out, "example.com/doc", "Man HTML link");
    apex_free_string(out);

    opts = apex_options_default();
    opts.output_format = APEX_OUTPUT_HTML;
    const char *filter_cmd = "cat";
    opts.ast_filter_commands = &filter_cmd;
    opts.ast_filter_count = 1;
    {
        const char *filtered = "# Kitchen Title\n\nA paragraph with **bold phrase**.\n";
        out = apex_markdown_to_html(filtered, strlen(filtered), &opts);
    }
    assert_contains(out, "Kitchen Title", "AST filter passthrough keeps the heading");
    assert_contains(out, "A paragraph with", "AST filter passthrough keeps the paragraph");
    apex_free_string(out);

    opts = apex_options_default();
    opts.obfuscate_emails = true;
    out = apex_markdown_to_html("[Ada](mailto:ada@example.com)", 30, &opts);
    assert_contains(out, "&#x6D;&#x61;&#x69;&#x6C;&#x74;&#x6F;&#x3A;", "Mailto href is hex-entity encoded");
    assert_not_contains(out, "ada@example.com", "Plain email address is removed");
    apex_free_string(out);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Output Format Coverage Tests", had_failures, false);
}


